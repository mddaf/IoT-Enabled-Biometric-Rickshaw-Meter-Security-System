#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <MPU6050.h>

// ========== CONFIGURATION ==========
// Lab Simulation Multiplier
// 5.0 means: 1 meter real drive = 5 meters on screen.
// (Change to 100.0 if you want 1m real = 100m virtual)
const float DEMO_MULTIPLIER = 5.0; 

const float WHEEL_CIRCUMFERENCE = 0.138; // 44mm Diameter * PI
const float FARE_RATE = 10.0;            // Taka per KM

// ========== PINS ==========
// Motor Driver
const int STBY = 8;
const int PWMA = 5, AIN1 = 6, AIN2 = 7;
const int PWMB = 9, BIN1 = 10, BIN2 = 11;

// Sensors
#define HALL_PIN 2       // Hall Effect Sensor (Interrupt)
#define RST_PIN 12       // RFID Reset
#define SS_PIN 13        // RFID SDA(SS)

// ========== OBJECTS ==========
// Fingerprint (Serial2: RX=17, TX=16 on Mega)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

// RFID
MFRC522 rfid(SS_PIN, RST_PIN);

// LCD (I2C Address 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// MPU6050 (I2C Address 0x68)
MPU6050 mpu; 

// ========== VARIABLES ==========
bool unlocked = false;
int speedVal = 200;

// Trip Variables
bool tripActive = false;
String startCardUID = "";
float totalDistance = 0;
volatile unsigned long pulseCount = 0;
volatile unsigned long lastPulse = 0;

// Rash Driving Variables
float rashAccel = 0;
bool rashWarning = false;
const float RASH_WARNING_G = 1.0; // Light shake
const float RASH_CRASH_G = 1.5;   // Hard shake (Immediate Stop)

// Interrupt Service Routine for Wheel Encoder
void hallISR() {
  unsigned long now = millis();
  if (now - lastPulse > 15) {  // 15ms debounce
    pulseCount++;
    lastPulse = now;
  }
}

void setup() {
  // 1. Initialize Communication
  Serial.begin(9600);     
  Serial1.begin(9600);    // Bluetooth (HC-05)
  Serial2.begin(9600);   // Fingerprint (Try 57600 or 9600 based on your sensor)
  Wire.begin();           // Hardware I2C (Pins 20 SDA, 21 SCL)
  SPI.begin();            // SPI Bus for RFID

  // 2. Initialize Sensors
  finger.begin(9600);
  rfid.PCD_Init();
  mpu.initialize();
  
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, FALLING);

  // 3. Initialize Motors
  pinMode(STBY, OUTPUT);
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  digitalWrite(STBY, HIGH);
  stopMotors();

  // 4. Initialize LCD
  lcd.init(); 
  lcd.backlight();
  
  // 5. System Check
  lcd.setCursor(0, 0); 
  if (mpu.testConnection()) lcd.print("MPU OK "); else lcd.print("MPU ERR ");
  if (finger.verifyPassword()) lcd.print("FP OK"); else lcd.print("FP ERR");
  delay(1500);

  lcd.clear();
  lcd.print("LAB MODE: 5x");
  delay(1500);
  
  lcdStatus("LOCKED - FP");
}

void loop() {
  // Continuous Safety Check
  checkRashDriving(); 
  
  // 1. RFID FARE SYSTEM
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getUID();
    
    if (!tripActive && unlocked) {
      // --- START TRIP ---
      startCardUID = uid;
      tripActive = true;
      pulseCount = 0; 
      totalDistance = 0;
      lcdStatus("Trip START");
      Serial.println("TRIP START: " + uid);
      delay(1000); 
    } 
    else if (uid == startCardUID) {
      // --- END TRIP ---
      calculateDistance();
      float fare = (totalDistance / 100.0) * FARE_RATE; 
      
      // Show Final Stats
      lcd.clear();
      lcd.print("Fare: Tk "); lcd.print(fare, 2);
      lcd.setCursor(0, 1);
      lcd.print("Dist: "); lcd.print(totalDistance, 0); lcd.print("m");
      
      Serial.print("END TRIP. Dist: "); Serial.print(totalDistance); 
      Serial.print("m | Fare: "); Serial.println(fare);
      
      tripActive = false;
      delay(5000); // Hold screen for teacher to see
    } 
    else {
      lcdStatus("Wrong Card!");
      delay(1000);
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // 2. FINGERPRINT UNLOCK
  if (!unlocked) {
    int id = getFingerprintID();
    if (id >= 0) {
      unlocked = true;
      lcdStatus("UNLOCKED ID:" + String(id));
      Serial.println("Unlocked by ID #" + String(id));
      delay(1500);
    } else {
      // If locked, keep updating status
      static unsigned long lockTimer = 0;
      if (millis() - lockTimer > 1000) {
        lcdStatus("LOCKED - FP");
        lockTimer = millis();
      }
      return; 
    }
  }

  // 3. BLUETOOTH CONTROL (Only works if unlocked)
  if (Serial1.available()) {
    char c = Serial1.read();
    switch (c) {
      case 'F': forward(speedVal); break;
      case 'B': backward(speedVal); break;
      case 'L': turnLeft(speedVal); break;
      case 'R': turnRight(speedVal); break;
      case 'S': stopMotors(); break;
      case 'K': // Lock command
        unlocked = false; 
        stopMotors(); 
        lcdStatus("LOCKED BY APP"); 
        break;
      case '0'...'9': 
        speedVal = map(c - '0', 0, 9, 0, 255); 
        break;
      case 'q': speedVal = 255; break; // Max speed
    }
  }

  // 4. DISPLAY UPDATES
  // To reduce flickering, update LCD only every 300ms
  static unsigned long lastLCDUpdate = 0;
  if (millis() - lastLCDUpdate > 300) {
    calculateDistance();
    showMainStatus();
    lastLCDUpdate = millis();
  }
}

// ========== FUNCTIONS ==========

void checkRashDriving() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az); 
  
  // Calculate G-Force
  rashAccel = sqrt((long)ax*ax + (long)ay*ay + (long)az*az) / 16384.0;
  
  if (rashAccel > RASH_WARNING_G) {
    rashWarning = true;
    
    // DEMO LOGIC: If shake is HARD (>2.5g), CRASH immediately.
    // This is much easier to demonstrate than waiting 5 seconds.
    if (rashAccel > RASH_CRASH_G) {
       stopMotors();
       lcd.clear();
       lcd.print("CRASH DETECTED!");
       lcd.setCursor(0,1);
       lcd.print("Force Stop");
       Serial.println("CRASH! Hard stop.");
       delay(3000); // Pause so teacher sees the crash message
       speedVal = 100; // Reset speed to slow
    }
  } else {
    rashWarning = false;
  }
}

void calculateDistance() {
  // Distance = Pulses * Wheel Circumference * Simulation Multiplier
  totalDistance = (pulseCount * WHEEL_CIRCUMFERENCE) * DEMO_MULTIPLIER;
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;
  return finger.fingerID;
}

void lcdStatus(String msg) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(msg);
}

void showMainStatus() {
  lcd.setCursor(0, 0);
  if (!unlocked) {
    lcd.print("LOCKED - FP     ");
  } else if (rashWarning) {
    lcd.print("WARN: "); lcd.print(rashAccel, 1); lcd.print("g     ");
  } else if (tripActive) {
    lcd.print("Dist: "); lcd.print((int)totalDistance); lcd.print("m   ");
  } else {
    lcd.print("UNLOCKED        ");
  }
  
  lcd.setCursor(0, 1);
  if (tripActive) {
     float currentFare = (totalDistance / 100.0) * FARE_RATE;
     lcd.print("Tk: "); lcd.print(currentFare, 2); lcd.print("     ");
  } else {
     lcd.print("Speed: "); lcd.print(speedVal); lcd.print("      ");
  }
}

// Motor Logic
void forward(int s) { digitalWrite(STBY, HIGH); digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); analogWrite(PWMA, s); digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); analogWrite(PWMB, s); }
void backward(int s) { digitalWrite(STBY, HIGH); digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH); analogWrite(PWMA, s); digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH); analogWrite(PWMB, s); }
void turnLeft(int s) { digitalWrite(STBY, HIGH); digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH); analogWrite(PWMA, s); digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); analogWrite(PWMB, s); }
void turnRight(int s) { digitalWrite(STBY, HIGH); digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); analogWrite(PWMA, s); digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH); analogWrite(PWMB, s); }
void stopMotors() { analogWrite(PWMA, 0); analogWrite(PWMB, 0); digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW); digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW); digitalWrite(STBY, LOW); }
