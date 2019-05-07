#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>
#include <Servo.h>

Servo ServoLeft, ServoRight;
SoftwareSerial NodeMCU(13,12); // rx, tx
SoftwareSerial FingerprintScanner(10, 11); // rx, tx
// intitiate fingerprint
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerprintScanner);

uint8_t id;

// nodemcu & fingerprint listener
bool isNodemcuListening = true;
bool isFingerListening = false;

// Fingerprint registration & verification
bool isEnrollFinger = false;
bool isVerifyFinger = false;

volatile int bills = 0, checkbills = 0;
volatile int coins = 0, checkcoins = 0;

const byte interruptPinBills = 2;
const byte interruptPinCoins = 3;

const int tiltPin = 46; //tilt pin
const int alarmRelayPin = 47; // alarm relay pin
const int lockRelayPin = 48; // lock relay pin
String alarmStatus = "";

// signing limited time
int timer = 50;
bool isTimerOn = false;

void setup() {
  // initialize Serial for testing
  Serial.begin(9600);

  // initialize nodemcu & fingerprint serial
  NodeMCU.begin(9600);
  FingerprintScanner.begin(57600);
  delay(100);
  pinMode(interruptPinCoins, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPinCoins), CoinSlotAcceptor, RISING);
  delay(100);
  pinMode(interruptPinBills, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPinBills), BillAcceptor, RISING);
  delay(100);

  pinMode(lockRelayPin, OUTPUT);
  digitalWrite(lockRelayPin, LOW); // turn lock as default
  pinMode(alarmRelayPin, OUTPUT);
  digitalWrite(alarmRelayPin, LOW); // turn off alarm as default

  pinMode(tiltPin, INPUT); //Tilt Switch
  digitalWrite(tiltPin, LOW); //Tilt Switch default state

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor");
    while (1) { delay(1); }
  }

  finger.getTemplateCount(); // get fingerprint count structure
  Serial.println("Sensor contains " + (String)finger.templateCount + " templates"); 
  Serial.println("Waiting for valid finger...");
  checkcoins =- 5;
  delay(2000);
}

/*
  ==================== Functions for Fingerprint ====================
*/

uint8_t verify_fingerprint() { // verification of fingerprint

  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK: Serial.println("Image taken"); break;
    case FINGERPRINT_NOFINGER: Serial.println("No finger detected"); return p;
    case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); return p;
    case FINGERPRINT_IMAGEFAIL: Serial.println("Imaging error"); return p;
    default: Serial.println("Unknown error"); return p;
  }

  // OK success!
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK: Serial.println("Image converted"); break;
    case FINGERPRINT_IMAGEMESS: Serial.println("Image too messy"); return p;
    case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); return p;
    case FINGERPRINT_FEATUREFAIL: Serial.println("Could not find fingerprint features"); return p;
    case FINGERPRINT_INVALIDIMAGE: Serial.println("Could not find fingerprint features"); return p;
    default: Serial.println("Unknown error"); return p;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) { 
    Serial.println("Found a print match!"); 
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) { 
    Serial.println("Communication error"); return p; 
  } else if (p == FINGERPRINT_NOTFOUND) { 
    send_message("SIGNIN_ERROR", "Did not find a match! Please try again.");
    timer = 50;
    Serial.println("Did not find a match"); return p; 
  } else { 
    Serial.println("Unknown error"); return p; 
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  timer = 50;
  isTimerOn = false;

  isVerifyFinger = false;
  isFingerListening = false;

  isNodemcuListening = true;

  send_fingerprint_id("SIGNIN_SUCCESS", (String)finger.fingerID);

  return finger.fingerID;
}

uint8_t readnumber(void) { // get template of the fingerprint database then add one
  uint8_t num = finger.templateCount + 1;
  while (num == 0) {
    while (! Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

void enroll_fingerprint(){ // registration of fingerprint
  
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 1000) you want to save this finger as...");
  id = readnumber();
  if (id == 0) {// ID #0 not allowed, try again!
      return;
  }
  Serial.print("Enrolling ID #");
  Serial.println(id);
  while (!  getFingerprintEnroll() );
}

uint8_t getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);

  int ctr = 50;

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK: Serial.println("Image taken"); break;
      case FINGERPRINT_NOFINGER: Serial.print("."); break;
      case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
      case FINGERPRINT_IMAGEFAIL: Serial.println("Imaging error"); break;
      default: Serial.println("Unknown error"); break;
    }

    ctr--;

    if (ctr <= 0) {
      isEnrollFinger = false;
      isFingerListening = false;
      isNodemcuListening = true;

      send_actions("ENROLL_CLOSE");
      return 1;
    }
  }

  // OK success!
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK: Serial.println("Image converted"); break;
    case FINGERPRINT_IMAGEMESS: Serial.println("Image too messy"); return p;
    case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); return p;
    case FINGERPRINT_FEATUREFAIL: Serial.println("Could not find fingerprint features"); return p;
    case FINGERPRINT_INVALIDIMAGE: Serial.println("Could not find fingerprint features"); return p;
    default: Serial.println("Unknown error"); return p;
  }
  
  Serial.println("Remove finger");
  send_message("ENROLL_INFO", "Remove your finger.");
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    Serial.print("*");
  }
  Serial.print("ID "); Serial.println(id);

  p = -1;
  Serial.println("Place same finger again");
  send_message("ENROLL_INFO", "Place same finger again.");

  ctr = 50;

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK: Serial.println("Image taken"); break;
      case FINGERPRINT_NOFINGER: Serial.print("#"); break;
      case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
      case FINGERPRINT_IMAGEFAIL: Serial.println("Imaging error"); break;
      default: Serial.println("Unknown error"); break;
    }

    ctr--;

    if (ctr <= 0) {
      isEnrollFinger = false;
      isFingerListening = false;
      isNodemcuListening = true;

      send_actions("ENROLL_CLOSE");
      return 1;
    }
  }

  // OK success!
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK: Serial.println("Image converted"); break;
    case FINGERPRINT_IMAGEMESS: Serial.println("Image too messy"); return p;
    case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); return p;
    case FINGERPRINT_FEATUREFAIL: Serial.println("Could not find fingerprint features"); return p;
    case FINGERPRINT_INVALIDIMAGE: Serial.println("Could not find fingerprint features"); return p;
    default: Serial.println("Unknown error"); return p;
  }
  
  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);
  
  p = finger.createModel();
  if (p == FINGERPRINT_OK) { Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error"); return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match"); 
    send_message("ENROLL_ERROR", "Fingerprints did not match.");
    return p;
  } else {
    Serial.println("Unknown error"); return p;
  }   
  
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");

    isEnrollFinger = false;
    isFingerListening = false;
    isNodemcuListening = true;

    send_fingerprint_id("ENROLL_SUCCESS", (String)id);
    return 1;

  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error"); return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location"); return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash"); return p;
  } else {
    Serial.println("Unknown error"); return p;
  }

}

/*
  ==================== Functions for Serial Listener ====================
*/

void nodemcu_listener() { // listener for nodemcu
  NodeMCU.listen();
  if (NodeMCU.available() > 0) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, NodeMCU);
    serializeJsonPretty(doc, Serial);

    if (doc["type"].as<String>() == "ENROLL_FINGERPRINT") {

      isFingerListening = true;
      isNodemcuListening = false;

      isEnrollFinger = true;
      isVerifyFinger = false;

    } else if (doc["type"].as<String>() == "VERIFY_FINGERPRINT") {

      isFingerListening = true;
      isNodemcuListening = false;

      isVerifyFinger = true;
      isEnrollFinger = false;

      timer = 50;
      isTimerOn = true;

    } else if (doc["type"].as<String>() == "ACTIVATE_BILL_COIN") {

      Serial.println("Currency Acceptors Activated.");

    } else if (doc["type"].as<String>() == "DEACTIVATE_BILL_COIN") {

      Serial.println("Currency Accecptors Deactivated.");

    } else if (doc["type"].as<String>() == "PURCHASE_ITEMS") {

      for (int i = 0; i < doc["size"]; i++) {
        Serial.println(doc["items"][i].as<String>());
        slotSelections(doc["items"][i]);
        send_actions("FALL_ITEMS");
      }
    } else if (doc["type"].as<String>() == "ALARM_OFF") {
      digitalWrite(alarmRelayPin, LOW);
      alarmStatus = "ALARM_OFF";
    } else if (doc["type"].as<String>() == "ALARM_ON") {
      digitalWrite(alarmRelayPin, HIGH);
      alarmStatus = "ALARM_ON";
    } else if (doc["type"].as<String>() == "LOCK_OFF") {
      digitalWrite(lockRelayPin, LOW);
    } else if (doc["type"].as<String>() == "LOCK_ON") {
      digitalWrite(lockRelayPin, HIGH);
    } else {
      isNodemcuListening = true;
      isFingerListening = false;
    }
  }
}

void fingerprint_listener() { // listener for fingerprint
  FingerprintScanner.listen();
  if (isVerifyFinger) {
    verify_fingerprint();
  } else if (isEnrollFinger) {
    enroll_fingerprint();
  }
}

void verify_finger_countdown() { // countdown for signing in

  if (isTimerOn) {
    timer = timer - 1;
    Serial.println("Time Left: " + (String)timer);
  }

  if (timer <= 0) {
    timer = 50;
    isTimerOn = false;

    isVerifyFinger = false;
    isFingerListening = false;

    isNodemcuListening = true;

    send_actions("SIGNIN_CLOSE");
  }

}


/*
  ==================== Function for data transmission with WebSockets ====================
*/
void send_cash(String type, int cash) {

  DynamicJsonDocument doc(1024);

  doc["type"] = type;
  doc["cash"] = cash;

  serializeJson(doc, NodeMCU);
}

void send_message(String type, String msg) {

  DynamicJsonDocument doc(1024);

  doc["type"] = type;
  doc["msg"] = msg;

  serializeJson(doc, NodeMCU);
}

void send_fingerprint_id(String type, String fid) {

  DynamicJsonDocument doc(1024);

  doc["type"] = type;
  doc["fid"] = fid;

  serializeJson(doc, NodeMCU);
}

void send_actions(String type) {
  
  DynamicJsonDocument doc(1024);

  doc["type"] = type;
  serializeJson(doc, NodeMCU);
}

/*
  ==================== Function for Currency Acceptors ====================
*/

void CoinSlotAcceptor() {
  if (digitalRead(interruptPinCoins) == LOW) {
    checkcoins++;
    send_message("CURRENCY_INFO", (String)checkcoins);
  }
}

void BillAcceptor() {
  if (digitalRead(interruptPinBills) == LOW) {
    checkbills++;
    send_message("CURRENCY_INFO", (String)checkbills);
  }
}

void CoinChecker(){

  if ((checkcoins >= 1) && (checkcoins <= 2)) {
    coins = 1;
    send_cash("ADD_CASH", coins);
    Serial.println(coins);
    coins = 0;
    checkcoins = 0;
  } else if ((checkcoins >= 3) && (checkcoins <= 6)) {
    coins = 5;
    send_cash("ADD_CASH", coins);
    Serial.println(coins);
    coins = 0;
    checkcoins = 0;
  } else if (checkcoins > 8) {
    coins = 10;
    send_cash("ADD_CASH", coins);
    Serial.println(coins);
    coins = 0;
    checkcoins = 0;
  } else if (checkcoins < 0) {
    Serial.println(coins);
    coins = 0;
    checkcoins = 0;
  }
}

void BillChecker(){

  if ((checkbills >= 18) && (checkbills < 30)) {
    bills = 20;
    send_cash("ADD_CASH", bills);
    Serial.println(bills);
    bills = 0;
    checkbills = 0;
  } else if (checkbills > 90) {
    bills = 100;
    send_cash("ADD_CASH", bills);
    Serial.println(bills);
    bills = 0;
    checkbills = 0;
  }

}

/*
  ==================== Function for Servos ====================
*/

void slotSelections(int slotsOn) {

  if (slotsOn == 1) {
    Serial.println("ONE");
    spinServo(1);
  } else if (slotsOn == 2) {
    Serial.println("TWO");
    spinServo(2);
  } else if (slotsOn == 3) {
    Serial.println("THREE");
    spinServo(3);
  } else if (slotsOn == 4) {
    Serial.println("FOUR");
    spinServo(4);
  } else {
    Serial.println("No more order left");
    slotsOn = 0;
  }
  delay(2000);
}

void spinServo(int Slotnumber) {
  switch (Slotnumber) {
  case 1:
    ServoLeft.attach(30);
    ServoRight.attach(31);
    break;
  case 2:
    ServoLeft.attach(32);
    ServoRight.attach(33);
    break;
  case 3:
    ServoLeft.attach(34);
    ServoRight.attach(35);
    break;
  case 4:
    ServoLeft.attach(36);
    ServoRight.attach(37);
    break;
  default: //nothing
    break;
  }

  ServoLeft.write(0);  // rotate
  ServoRight.write(0); // rotate
  delay(1300);
  ServoLeft.write(90); // stop
  ServoLeft.detach();
  ServoRight.write(90); // stop
  ServoRight.detach();
  Serial.println("servo Spins");
}

void tilt_sensor() {

  //Serial.println(digitalRead(tiltPin));
  if (digitalRead(tiltPin) == HIGH) {
    digitalWrite(alarmRelayPin, HIGH); //turn the alarm off
    send_actions("ALARM_ON");
  } else if(alarmStatus == "ALARM_OFF") {                                   ////if tilt switch breakover
    digitalWrite(alarmRelayPin, LOW); //turn alarm led on
    send_actions("ALARM_LOW");
    alarmStatus = "";
  }else if(alarmStatus == "ALARM_ON"){
    digitalWrite(alarmRelayPin, HIGH); //turn the alarm off
    send_actions("ALARM_ON");
  }
}

void RunCodeInMillis(){
  static unsigned long timer = millis();
  static int deciSeconds1 = 0, deciSeconds2 = 0;

  if (millis() - timer >= 100) {
    timer += 50;

    CoinSlotAcceptor();
    BillAcceptor();

    if (deciSeconds1 >= checkbills * 1.35) {
      BillChecker();
      deciSeconds1 = 0;
    } if (deciSeconds2 >= checkcoins * 3) {
      CoinChecker();
      deciSeconds2 = 0;
    }

    deciSeconds1++;
    deciSeconds2++;
    
    tilt_sensor();

  }
}

void loop() {
  if (isNodemcuListening) {
    nodemcu_listener();
    // Serial.println("Listening NodeMCU");
  } else if (isFingerListening) {
    fingerprint_listener();
    // Serial.println("Listening Fingerprint");
  }
  verify_finger_countdown();
  RunCodeInMillis();
}
