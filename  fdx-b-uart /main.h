#include <Arduino.h>
#include <WiFi.h>
#include "stepper_control.h"
#include "rfid_control.h"
#include "state.h"

void setup() {
  // Initialize serial communication
  Serial.begin(115200);  // Debug serial
  Serial1.begin(9600);   // RFID reader serial - typically 9600 baud for many RFID readers
  
  // Wait for serial connection to be established
  delay(1000);
  debugPrint("Starting ESP32 RFID and Stepper Control");
  debugPrint("Initializing with RFID EM4305 FDX-B Animal Tag Reader Module");
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STEPPER_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize stepper motor control pins
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);
  pinMode(STEPPER_ENA, OUTPUT);
  pinMode(STEPPER_ENB, OUTPUT);
  
  // Initialize L298N enable pins to LOW (disabled) initially
  digitalWrite(STEPPER_ENA, LOW);
  digitalWrite(STEPPER_ENB, LOW);
  
  // Configure stepper motor parameters
  stepper.setMaxSpeed(500.0);     // Maximum speed in steps per second
  stepper.setAcceleration(200.0); // Acceleration in steps per second per second
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);  // Allocate timer 0 for ESP32 servo
  myServo.setPeriodHertz(50);  // Standard 50Hz servo
  myServo.attach(SERVO_PIN, 500, 2400); // Attaches the servo to the specified pin with min/max pulse width
  
  // Test servo operation during setup
  debugPrint("Testing servo operation...");
  myServo.write(SERVO_CLOSED_POS);
  debugPrint("Servo at closed position");
  delay(1000);
  myServo.write(SERVO_OPEN_POS);
  debugPrint("Servo at open position");
  delay(1000);
  myServo.write(SERVO_CLOSED_POS);
  debugPrint("Servo back to closed position");
  
  // Connect to WiFi for time synchronization
  debugPrint("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int wifiRetries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) {
    delay(500);
    Serial.print(".");
    wifiRetries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi connected!");
    debugPrint(WiFi.localIP().toString().c_str());
    
    // Initialize time from NTP server
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    debugPrint("Time synchronized");
    
    // Display current time
    String timeStr = getTimeString();
    debugPrint(("Current time: " + timeStr).c_str());
  } else {
    debugPrint("WiFi connection failed!");
  }
  
  // Flash LED to indicate successful initialization
  debugPrint("Testing LED indicator...");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  // Test RFID reader connection
  debugPrint("Testing RFID reader connection on Serial1...");
  
  // Flush any existing data
  while (Serial1.available()) {
    Serial1.read();
  }
  
  // Some RFID readers respond to simple commands
  // Try sending a carriage return which often triggers a response
  Serial1.write('\r');
  delay(500);
  
  // Check for any response
  boolean readerResponded = false;
  int bytesAvailable = Serial1.available();
  
  if (bytesAvailable > 0) {
    debugPrint("RFID reader responded with data:");
    for (int i = 0; i < min(bytesAvailable, 20); i++) { // Read up to 20 bytes
      byte b = Serial1.read();
      char bufferStr[20];
      sprintf(bufferStr, "Byte %d: 0x%02X (%c)", i, b, (b >= 32 && b <= 126) ? (char)b : '?');
      debugPrint(bufferStr);
    }
    readerResponded = true;
  } else {
    debugPrint("No initial response from RFID reader");
  }
  
  // Try again with different command if first attempt failed
  if (!readerResponded) {
    debugPrint("Trying alternative command...");
    // Some readers respond to ASCII 'V' for version info
    Serial1.write('V');
    delay(500);
    
    bytesAvailable = Serial1.available();
    if (bytesAvailable > 0) {
      debugPrint("RFID reader responded to alternative command:");
      for (int i = 0; i < min(bytesAvailable, 20); i++) {
        byte b = Serial1.read();
        char bufferStr[20];
        sprintf(bufferStr, "Byte %d: 0x%02X (%c)", i, b, (b >= 32 && b <= 126) ? (char)b : '?');
        debugPrint(bufferStr);
      }
      readerResponded = true;
    }
  }
  
  // Final reader status
  if (readerResponded) {
    debugPrint("RFID reader detected and responding on Serial1");
  } else {
    debugPrint("WARNING: RFID reader not responding. Check connections and power.");
    
    // Check electrical connection by monitoring for any activity
    debugPrint("Monitoring for any RFID reader activity for 3 seconds...");
    unsigned long startTime = millis();
    boolean anyActivity = false;
    
    while (millis() - startTime < 3000) {
      if (Serial1.available() > 0) {
        anyActivity = true;
        debugPrint("Activity detected on Serial1!");
        break;
      }
      delay(100);
    }
    
    if (!anyActivity) {
      debugPrint("No activity detected on Serial1. Possible issues:");
      debugPrint("1. Reader not powered - check 5V connection");
      debugPrint("2. TXD not connected to ESP32 TX1 pin");
      debugPrint("3. Incorrect baud rate - try different speeds");
      debugPrint("4. Reader hardware failure");
    }
  }
  
  // Print pin connection reminder
  debugPrint("RFID Reader Connection Reminder:");
  debugPrint("- 5V pin to 5V power");
  debugPrint("- GND pin to GND");
  debugPrint("- TXD pin to ESP32 TX1 pin (DATA1_PIN)");
  
  debugPrint("System Ready - Waiting for RFID tags");
}

void loop() {
  // Current time
  unsigned long currentMillis = millis();
  
  // Process any incoming RFID data
  processRFIDData();
  
  // Check button state for manual servo operation
  checkServoButton();
  
  // Check button state for manual stepper operation
  if (currentMillis - lastButtonCheck >= BUTTON_CHECK_INTERVAL) {
    lastButtonCheck = currentMillis;
    checkStepperButton();
  }
  
  // Check scheduled tasks (time-based operations)
  if (currentMillis - lastTimeCheck >= TIME_CHECK_INTERVAL) {
    lastTimeCheck = currentMillis;
    checkScheduledTasks();
  }
  
  // Handle stepper motor movement if scheduled
  if (stepperScheduled) {
    startStepperRotation();
    stepperScheduled = false;
    stepperRotating = true;
  }
  
  // Update stepper motor state
  if (stepper.isRunning() || buttonControlActive) {
    stepper.run();  // Step the motor if required
    
    // Check if stepper has completed its movement
    if (!stepper.isRunning() && stepperRotating && !buttonControlActive) {
      debugPrint("Stepper rotation complete");
      stepperRotating = false;
      disableStepperMotor();  // Disable motor when done to save power
    }
  }
  
  // Short delay to prevent CPU hogging
  delay(5);
}