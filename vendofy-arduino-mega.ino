#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>

SoftwareSerial NodeMCU(13,12);
SoftwareSerial FingerprintScanner(10, 11);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerprintScanner);

uint8_t id;

bool isNodemcuListening = true;
bool isFingerListening = false;

bool isEnrollFinger = false;
bool isVerifyFinger = false;

int timer = 50;
bool isTimerOn = false;

void setup() {
  // initialize ports
  Serial.begin(9600);
  
  NodeMCU.begin(9600);
  FingerprintScanner.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor");
    while (1) { delay(1); }
  }

  finger.getTemplateCount(); // get fingerprint count structure
  Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  Serial.println("Waiting for valid finger...");
  delay(10);
}

uint8_t getFingerprintID() { 

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
    send_message("SIGNIN", "ERROR_MESSAGE", "Did not find a match! Try Again.");
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

  timer = 1;
  send_message("SIGNIN", "SIGN_IN_SUCCESS", (String)finger.fingerID);

  return finger.fingerID;
}

int getFingerprintIDez() { // returns -1 if failed, otherwise returns ID #
  
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
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

void enrollFinger(){
  
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
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK: Serial.println("Image taken"); break;
      case FINGERPRINT_NOFINGER: Serial.print("."); break;
      case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
      case FINGERPRINT_IMAGEFAIL: Serial.println("Imaging error"); break;
      default: Serial.println("Unknown error"); break;
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
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK: Serial.println("Image taken"); break;
      case FINGERPRINT_NOFINGER: Serial.print("*"); break;
      case FINGERPRINT_PACKETRECIEVEERR: Serial.println("Communication error"); break;
      case FINGERPRINT_IMAGEFAIL: Serial.println("Imaging error"); break;
      default: Serial.println("Unknown error"); break;
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
    Serial.println("Fingerprints did not match"); return p;
  } else {
    Serial.println("Unknown error"); return p;
  }   
  
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!-----------");
    isVerifyFinger = false;
    isEnrollFinger = false;
    isFingerListening = false;
    isNodemcuListening = true;
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

void test_code(){
  
  getFingerprintID();
  if (finger.fingerID != 0)
  { Serial.println("Your Fingerprint is already existed"); }
  else
  { Serial.println("Fingerprint not existed set fingerprint to enroll"); }
}

void nodemcu_listener() {
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

      isTimerOn = true;

    } else {
      isNodemcuListening = true;
      isFingerListening = false;
    }
  }
}

void fingerprint_listener() {
  FingerprintScanner.listen();
  if (isVerifyFinger) {
    getFingerprintID();
  } else if (isEnrollFinger) {
    enrollFinger();
  }
}

void verify_finger_countdown() {

  if (isTimerOn) {
    timer = timer - 1;
    Serial.println("Time Left: " + (String)timer);
  }

  if (timer <= 0) {
    isTimerOn = false;
    timer = 50;

    isVerifyFinger = false;
    isEnrollFinger = false;
    isFingerListening = false;

    isNodemcuListening = true;

    send_message("SIGNIN", "CLOSE_SIGN_IN", "Signing In has timeout.");
  }

}

void send_message(String type, String status, String msg) {

  DynamicJsonDocument doc(1024);

  doc["type"] = type;
  doc["status"] = status;
  doc["msg"] = msg;

  serializeJson(doc, NodeMCU);
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
}
