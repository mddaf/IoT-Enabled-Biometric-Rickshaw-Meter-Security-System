# 🛺 IoT-Enabled Biometric Rickshaw Meter & Security System

<div align="center">

![Arduino](https://img.shields.io/badge/Arduino-Mega%202560-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Course](https://img.shields.io/badge/Course-CSE360-blueviolet?style=for-the-badge)
![University](https://img.shields.io/badge/BRAC%20University-Group%2007-orange?style=for-the-badge)

*A fully offline, embedded-hardware smart meter that brings biometric security, automated fare calculation, and crash detection to traditional rickshaws.*

</div>

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [System Architecture](#-system-architecture)
- [Hardware Components & Cost](#-hardware-components--cost)
- [Wiring & Pin Configuration](#-wiring--pin-configuration)
- [Communication Protocols](#-communication-protocols)
- [Software Design](#-software-design)
- [How It Works](#-how-it-works)
- [Installation & Setup](#-installation--setup)
- [Demo & Results](#-demo--results)
- [Challenges & Solutions](#-challenges--solutions)
- [Limitations & Future Work](#-limitations--future-work)
- [Team](#-team)
- [References](#-references)

---

## 🔍 Overview

Traditional rickshaw services suffer from three key problems:

| Problem | Impact |
|---|---|
| No anti-theft / unauthorized use | Rickshaws stolen or used without owner consent |
| Manual fare negotiation | Disputes, overcharging, passenger distrust |
| Reckless driving with no accountability | Passenger safety risk, accidents |

This project addresses all three with a single embedded system built on an **Arduino Mega**, integrating biometric authentication, RFID trip management, Hall Effect distance sensing, and accelerometer-based crash detection — **no internet required**.

> **Course:** CSE360 – Computer Interfacing, BRAC University  
> **Group:** 07 · Section: 09  
> **Submission Date:** January 6, 2026

---

## ✨ Features

### 🔐 Biometric Security Lock
- The vehicle's drive motors are **disabled by default**
- Only a **registered fingerprint** (via Adafruit R307 sensor over UART) unlocks motor control
- Remote software lock available via Bluetooth (`K` command)

### 🎫 RFID Trip Management & Fare Calculation
- Tap a registered RFID card (MFRC522 / RC522) to **start** a trip
- Tap the **same card** again to **end** the trip and display the final fare
- Tapping a **different card** mid-trip shows a "Wrong Card!" error
- Fare formula: `Fare (Tk) = (Distance in meters / 100) × 10 Tk/km`

### 📏 Real-Time Distance Measurement
- A **Hall Effect sensor (A3144)** on the wheel counts magnetic pulses via hardware interrupt
- Distance calculated as:  
  `Distance = Pulse Count × Wheel Circumference (0.138 m) × Demo Multiplier (5×)`
- The 5× demo multiplier simulates real distances in a constrained lab space

### 🚨 Rash Driving & Crash Detection
- **MPU6050** accelerometer continuously monitors G-force
- **Warning threshold (>1.0g):** Displays a caution warning on LCD
- **Crash threshold (>1.5g):** Immediately cuts motor power, shows "CRASH DETECTED!", and logs to serial — motors resume at reduced speed after 3 seconds

### 📱 Bluetooth Remote Control (HC-05)
- Full directional control over Bluetooth (Forward / Backward / Left / Right / Stop)
- Speed adjustment from 0–9 (mapped to 0–255 PWM) plus max speed (`q`)
- Only active when the system is **unlocked** via fingerprint

### 📟 Live LCD Status Display
- 16×2 I2C LCD updates every 300ms showing:
  - Lock/unlock state
  - Active trip distance (meters)
  - Running fare (Tk)
  - G-force warning level
  - Current motor speed

---

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Arduino Mega 2560                    │
│                                                         │
│  ┌──────────┐  UART   ┌───────────────────────────┐    │
│  │  HC-05   │◄───────►│  Serial1 (RX1/TX1)         │    │
│  │Bluetooth │         │  Pin 18 (TX), Pin 19 (RX)  │    │
│  └──────────┘         └───────────────────────────┘    │
│                                                         │
│  ┌──────────┐  UART   ┌───────────────────────────┐    │
│  │  R307    │◄───────►│  Serial2 (RX2/TX2)         │    │
│  │Fingerprint│        │  Pin 16 (TX), Pin 17 (RX)  │    │
│  └──────────┘         └───────────────────────────┘    │
│                                                         │
│  ┌──────────┐         ┌───────────────────────────┐    │
│  │  RC522   │◄── SPI ►│  Pins 50 (MISO),           │    │
│  │   RFID   │         │  51 (MOSI), 52 (SCK),      │    │
│  └──────────┘         │  13 (SS), 12 (RST)         │    │
│                        └───────────────────────────┘    │
│                                                         │
│  ┌──────────┐         ┌───────────────────────────┐    │
│  │MPU6050   │◄── I2C ►│  SDA (Pin 20)              │    │
│  │LCD 16×2  │         │  SCL (Pin 21)              │    │
│  └──────────┘         └───────────────────────────┘    │
│                                                         │
│  ┌──────────┐  INT    ┌───────────────────────────┐    │
│  │  A3144   │────────►│  Pin 2 (Hardware Interrupt)│    │
│  │Hall Sens.│         └───────────────────────────┘    │
│  └──────────┘                                          │
│                                                         │
│  ┌────────────────┐   ┌───────────────────────────┐    │
│  │ TB6612FNG      │◄──│  Pins 5–11 (PWM + DIR)     │    │
│  │ Motor Driver   │   └───────────────────────────┘    │
│  └────┬───────────┘                                    │
│       │ Motor A + Motor B                               │
└───────┼─────────────────────────────────────────────────┘
        ▼
   [DC Motors × 2]
```

### Program Flowchart

```
START
  │
  ▼
System Init (Serial, I2C, SPI, sensors, motors, LCD)
  │
  ▼
  ┌────────────────────────────────────────────────┐
  │                  MAIN LOOP                     │
  │                                                │
  │  1. checkRashDriving()  ◄── Always runs first  │
  │     • >1.0g → warning flag                    │
  │     • >1.5g → CRASH → stop motors, 3s pause   │
  │                                                │
  │  2. RFID Check                                 │
  │     • New card + unlocked + no trip → START    │
  │     • Same card + trip active → END + fare     │
  │     • Different card → "Wrong Card!"           │
  │                                                │
  │  3. Fingerprint Check (if locked)              │
  │     • Match → unlocked = true                  │
  │     • No match → show "LOCKED - FP", return    │
  │                                                │
  │  4. Bluetooth Commands (if unlocked)           │
  │     F/B/L/R/S → motor control                  │
  │     K → lock system                            │
  │     0-9/q → speed control                      │
  │                                                │
  │  5. LCD Update (every 300ms)                   │
  └────────────────────────────────────────────────┘
```

---

## 🧰 Hardware Components & Cost

| Component | Qty | Unit Price (BDT) | Total (BDT) |
|---|---|---|---|
| Arduino Mega / Uno | 1 | 1,800.00 | 1,800.00 |
| Fingerprint Sensor (R307) with UART Module | 1 | 2,100.00 | 2,100.00 |
| MFRC522 RFID Module | 1 | 175.00 | 175.00 |
| MPU6050 Accelerometer / Gyroscope | 1 | 250.00 | 250.00 |
| Hall Effect Sensor (A3144) | 1 | 60.00 | 60.00 |
| TB6612FNG Motor Driver | 1 | 219.00 | 219.00 |
| HC-05 Bluetooth Module | 1 | 380.00 | 380.00 |
| LCD 16×2 (I2C, 0x27) | 1 | 170.00 | 170.00 |
| Jumper Wire Bundle | 1 | 120.00 | 120.00 |
| Li-ion Battery 3.7V | 3 | 95.00 | 285.00 |
| Chassis | 1 | 400.00 | 400.00 |
| N20 Wheels | 2 | 80.00 | 160.00 |
| DC Motors | 2 | 295.00 | 590.00 |
| Magnet (for Hall Sensor) | 1 | 25.00 | 25.00 |
| **Grand Total** | | | **6,734.00** |

---

## 🔌 Wiring & Pin Configuration

### Motor Driver (TB6612FNG)

| Driver Pin | Arduino Mega Pin | Description |
|---|---|---|
| STBY | Pin 8 | Standby / Enable |
| PWMA | Pin 5 | Speed control — Motor A |
| AIN1 | Pin 6 | Direction — Motor A (1) |
| AIN2 | Pin 7 | Direction — Motor A (2) |
| PWMB | Pin 9 | Speed control — Motor B |
| BIN1 | Pin 10 | Direction — Motor B (1) |
| BIN2 | Pin 11 | Direction — Motor B (2) |
| VM | Battery (+) | Motor power (7V–12V) |
| GND | Battery (−) | Motor ground |

### I2C Bus — LCD & MPU6050

| Component | Pin | Arduino Mega Pin |
|---|---|---|
| LCD 16×2 (I2C) | SDA | Pin 20 |
| | SCL | Pin 21 |
| | VCC | 5V |
| | GND | GND |
| MPU6050 | SDA | Pin 20 |
| | SCL | Pin 21 |
| | VCC | 5V |
| | GND | GND |

> Both devices share the same I2C bus. LCD address: `0x27`, MPU6050 address: `0x68`.

### RFID Module (RC522) — SPI

| RFID Pin | Arduino Mega Pin | Note |
|---|---|---|
| SDA (SS) | Pin 13 | Chip select |
| SCK | Pin 52 | Hardware SPI |
| MOSI | Pin 51 | Hardware SPI |
| MISO | Pin 50 | Hardware SPI |
| RST | Pin 12 | Reset |
| 3.3V | 3.3V | ⚠️ **DO NOT use 5V** |
| GND | GND | |

### Fingerprint Sensor (R307) — UART

| Sensor Pin | Arduino Mega Pin | Note |
|---|---|---|
| VCC (Red) | 5V | |
| GND (Black) | GND | |
| TX (Green) | Pin 17 (RX2) | Sensor TX → Arduino RX |
| RX (White) | Pin 16 (TX2) | Sensor RX → Arduino TX |

### Bluetooth Module (HC-05) — UART

| HC-05 Pin | Arduino Mega Pin | Note |
|---|---|---|
| VCC | 5V | |
| GND | GND | |
| TXD | Pin 19 (RX1) | BT TX → Arduino RX |
| RXD | Pin 18 (TX1) | BT RX → Arduino TX |

### Hall Effect Sensor (A3144) — Digital Interrupt

| Sensor Pin | Arduino Mega Pin | Note |
|---|---|---|
| Pin 1 (VCC) | 5V | Face sensor toward you |
| Pin 2 (GND) | GND | Middle pin |
| Pin 3 (OUT) | Pin 2 | Hardware interrupt (INT0) |

---

## 📡 Communication Protocols

| Protocol | Used For | Arduino Interface |
|---|---|---|
| **UART** | Fingerprint Sensor (R307) | `Serial2` — TX2 (Pin 16), RX2 (Pin 17) |
| **UART** | Bluetooth (HC-05) | `Serial1` — TX1 (Pin 18), RX1 (Pin 19) |
| **I2C** | LCD Display + MPU6050 | `Wire` — SDA (Pin 20), SCL (Pin 21) |
| **SPI** | RFID (RC522) | Hardware SPI — Pins 50, 51, 52, SS=13, RST=12 |
| **Digital Interrupt** | Hall Effect Sensor | `attachInterrupt` on Pin 2 (INT0), FALLING edge |

---

## 💻 Software Design

### Libraries Required

Install the following via the **Arduino Library Manager** or manually:

```
Adafruit_Fingerprint  (by Adafruit)
LiquidCrystal_I2C     (by Frank de Brabander)
MFRC522               (by miguelbalboa)
MPU6050               (by Electronic Cats / Jeff Rowberg)
Wire                  (built-in)
SPI                   (built-in)
```

### Key Constants

```cpp
const float DEMO_MULTIPLIER     = 5.0;    // 1m real = 5m virtual (lab simulation)
const float WHEEL_CIRCUMFERENCE = 0.138;  // 44mm diameter × π (meters)
const float FARE_RATE           = 10.0;   // Taka per 100 meters

const float RASH_WARNING_G = 1.0;  // Warn above this G-force
const float RASH_CRASH_G   = 1.5;  // Hard stop above this G-force
```

### Distance Calculation

```
Distance (m) = pulseCount × 0.138 m × 5.0
```

Each magnet pass triggers one ISR pulse. The wheel circumference gives distance per revolution.

### Fare Calculation

```
Fare (Tk) = (Distance in meters ÷ 100) × 10
```

*Example: 200 m virtual distance → Fare = 20.00 Tk*

### ISR (Interrupt Service Routine) — Hall Sensor Debounce

```cpp
void hallISR() {
    unsigned long now = millis();
    if (now - lastPulse > 15) {  // 15ms software debounce
        pulseCount++;
        lastPulse = now;
    }
}
```

### Bluetooth Command Reference

| Command Char | Action |
|---|---|
| `F` | Move Forward |
| `B` | Move Backward |
| `L` | Turn Left |
| `R` | Turn Right |
| `S` | Stop Motors |
| `K` | Lock System (stop + disable motors) |
| `0`–`9` | Set speed (maps 0–9 → PWM 0–255) |
| `q` | Maximum speed (PWM 255) |

### Crash Detection Logic

```cpp
rashAccel = sqrt(ax² + ay² + az²) / 16384.0;  // Convert raw to G

if (rashAccel > RASH_CRASH_G) {
    stopMotors();
    // Display "CRASH DETECTED!" for 3 seconds
    // Reset speed to 100 (slow) on resume
} else if (rashAccel > RASH_WARNING_G) {
    rashWarning = true;  // Show warning on LCD
}
```

---

## 🚀 Installation & Setup

### Prerequisites

- Arduino IDE 1.8+ or Arduino IDE 2.x
- Arduino Mega 2560 board definition installed
- All libraries listed above installed

### Steps

**1. Clone this repository**
```bash
git clone https://github.com/your-username/iot-rickshaw-meter.git
cd iot-rickshaw-meter
```

**2. Open the sketch**

Open `CSE360_Project_Code.ino` in the Arduino IDE.

**3. Select your board and port**
```
Tools → Board → Arduino Mega or Mega 2560
Tools → Port  → (your COM port)
```

**4. Install libraries**

Go to `Sketch → Include Library → Manage Libraries` and install:
- `Adafruit Fingerprint Sensor Library`
- `LiquidCrystal I2C`
- `MFRC522`
- `MPU6050` (by Electronic Cats)

**5. Enroll fingerprints**

You must enroll at least one fingerprint before the system can unlock. Use Adafruit's [fingerprint enroll example](https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library/blob/master/examples/enroll/enroll.ino) sketch first, then switch back to the main sketch.

**6. Upload**

Click **Upload** (→). Open Serial Monitor at **9600 baud** to watch status messages.

**7. Wire the circuit**

Follow the [Wiring section](#-wiring--pin-configuration) above exactly. Double-check:
- RFID module is on **3.3V** (not 5V)
- Hall sensor magnet is mounted on the wheel, sensor aligned correctly
- I2C address of LCD is `0x27` (some modules use `0x3F` — update the code if needed)

---

## 📊 Demo & Results

| Test | Result |
|---|---|
| Fingerprint unlock (registered ID) | ✅ Motors enabled, LCD shows "UNLOCKED ID:X" |
| Fingerprint unlock (unregistered ID) | ✅ Motors stay locked, LCD stays "LOCKED - FP" |
| RFID trip start (unlocked) | ✅ Trip session begins, pulse counter resets |
| RFID trip end (same card) | ✅ Fare and distance displayed on LCD for 5 seconds |
| RFID trip end (wrong card) | ✅ "Wrong Card!" displayed, trip continues |
| Hall sensor — 200m virtual distance | ✅ Fare = **20.00 Tk** (10 Tk/100m) |
| Crash detection (>1.5g shake) | ✅ Motors cut within **300ms**, "CRASH DETECTED!" on LCD |
| Bluetooth control (unlocked) | ✅ F/B/L/R/S commands execute correctly |
| Bluetooth control (locked) | ✅ Commands ignored |
| LCD update cycle | ✅ Refreshes every 300ms without flicker |

---

## ⚠️ Challenges & Solutions

### 1. Hall Sensor Bouncing
**Problem:** The Hall sensor triggered multiple pulses per magnet pass due to contact bounce, inflating distance readings.

**Fix:** Added a 15ms software debounce inside the ISR using `millis()`:
```cpp
if (now - lastPulse > 15) { pulseCount++; lastPulse = now; }
```

### 2. RFID Blocking the Main Loop
**Problem:** Blocking RFID reads caused the MPU6050 safety check to pause, meaning crash detection could be delayed.

**Fix:** Used non-blocking PICC checks (`PICC_IsNewCardPresent()`) so the safety function runs every loop iteration regardless.

### 3. LCD Flickering
**Problem:** Calling `lcd.clear()` every loop iteration caused visible flicker on the 16×2 display.

**Fix:** Moved LCD updates inside a 300ms timer gate using `millis()`, and used `lcd.setCursor()` to overwrite individual positions instead of clearing the full screen.

### 4. Fingerprint Sensor Baud Rate
**Problem:** The R307 sensor defaulted to 57600 baud on some units but 9600 on others.

**Fix:** `Serial2.begin(9600)` — change to `57600` if the sensor fails verification (`FP ERR` on startup LCD).

---

## 🔮 Limitations & Future Work

### Current Limitations
- Distance simulation uses a 5× multiplier (lab space constraint) — not real GPS
- Optical fingerprint sensor struggles with wet or dirty fingers (rain conditions)
- No persistent data storage: trip history is lost on power cycle

### Planned Improvements
- **GSM (SIM800L):** Send SMS alerts to the owner on crash detection
- **Regenerative braking:** Recharge the battery during deceleration for better sustainability
- **EEPROM logging:** Store trip history locally even after power loss
- **GPS module:** Replace demo multiplier with real coordinate-based distance tracking
- **Capacitive fingerprint sensor:** More reliable in wet conditions than optical sensors

---

## 👥 Team

**Group 07 — Section 09, CSE360, BRAC University**

| Name | Student ID | Role | Contributions |
|---|---|---|---|
| **Md. Dodi Al Fayed** | 22301325 | Hardware Design & Circuit Building | Designed the circuit schematic, soldered motor driver connections, assembled the physical chassis |
| **Jauad Ahmed Sadik** | 22301342 | Software Design | Wrote C++ code for logic and interrupts, implemented Hall sensor debounce, integrated the Adafruit Fingerprint library |
| **Md. Sabbir Akon** | 22301242 | Calibration & Report | Calibrated MPU6050 crash thresholds, designed flowchart and block diagrams, compiled the final report and cost analysis |

---

## 📚 References

- Adafruit Industries. *Adafruit Optical Fingerprint Sensor*. https://www.adafruit.com/product/751
- LastMinuteEngineers. *Interface MPU6050 Accelerometer & Gyroscope Sensor with Arduino*. https://lastminuteengineers.com/mpu6050-accel-gyro-arduino-tutorial/
- NXP Semiconductors. (2016). *MFRC522 Standard Performance MIFARE and NTAG Frontend Data Sheet*. https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf
- Adafruit Fingerprint Sensor Library. https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library
- miguelbalboa MFRC522 Library. https://github.com/miguelbalboa/rfid

---

<div align="center">

Made with ❤️ for CSE360 — BRAC University · 2026

</div>
