#include <ESP32Servo.h>      // Include ESP32Servo library instead of Servo.h
#include <AccelStepper.h>    // Include the AccelStepper library
#include <WiFi.h>
#include "state.h"
#include "stepper_control.h"
#include "rfid_control.h"
#include "web_server.h"

// WiFi configuration
const char* webServerSSID = "NetWatch";       // Using the same SSID from your code
const char* webServerPassword = "Jcd26044";   // Using the same password from your code

// Create the web server instance
TaskSchedulerWebServer taskServer(webServerSSID, webServerPassword);

// Add this function before setup()
void setupWebServer() {
  // Initialize the web server
  if (taskServer.begin()) {
    debugPrint("Task Scheduler Web Server started!");
    Serial.print("Access the scheduler at http://");
    Serial.println(taskServer.getIP());
  } else {
    debugPrint("Failed to start Task Scheduler Web Server");
  }
}

void setup() {
  // Use Serial1 for debug output since pins 0 and 1 are used for RFID
  Serial1.begin(9600);  // Use UART1 for debug output

  // Redefine Serial to use Serial1 for all debug output
  #define Serial Serial1

  // Configure pins
  pinMode(DATA0_PIN, INPUT_PULLUP);    // Add pull-up to help with noise
  pinMode(DATA1_PIN, INPUT_PULLUP);    // Add pull-up to help with noise
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);            // Set LED pin as output
  pinMode(LED_BUTTON_PIN, INPUT_PULLUP);      // Button with pull-up
  pinMode(STEPPER_BUTTON_PIN, INPUT_PULLUP);  // Button with pull-up

  // Turn off LED initially
  digitalWrite(LED_PIN, LOW);

  // Setup stepper motor enable pins
  pinMode(STEPPER_ENA, OUTPUT);
  pinMode(STEPPER_ENB, OUTPUT);
  digitalWrite(STEPPER_ENA, LOW);  // Start with motor disabled
  digitalWrite(STEPPER_ENB, LOW);  // Start with motor disabled

  // Setup servo
  ESP32PWM::allocateTimer(0);  // Allocate timer 0 for ESP32 servo
  myServo.setPeriodHertz(50);  // Standard 50Hz servo
  myServo.attach(SERVO_PIN, 500, 2400);  // Attach the servo with min/max pulse width
  myServo.write(SERVO_CLOSED_POS);  // Start in closed position

  // Configure stepper motor
  stepper.setMaxSpeed(500);          // Maximum speed in steps per second
  stepper.setAcceleration(200);      // Acceleration in steps per second per second
  stepper.setSpeed(200);             // Initial speed in steps per second

  debugPrint("RFID Reader for Arduino Nano ESP32 with Servo and AccelStepper - Starting up...");

  // Test LED to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  // Test servo movement
  debugPrint("Testing servo...");
  myServo.write(SERVO_OPEN_POS);
  delay(500);
  myServo.write(SERVO_CLOSED_POS);
  delay(500);

  // Test stepper motor (small movement)
  debugPrint("Testing stepper motor...");
  enableStepperMotor(); // Enable motor for testing
  stepper.move(20);  // Move 20 steps
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  delay(500);
  stepper.move(-20);  // Move back 20 steps
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  delay(500);
  disableStepperMotor(); // Disable motor after testing

  // Initialize WiFi and time
  debugPrint("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  // Wait for WiFi connection - timeout after 20 seconds
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(1000);
    Serial.print(".");
    wifiTimeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize and sync time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Print current time
    Serial.print("Current time: ");
    Serial.println(getTimeString());

    setupWebServer();

    // Disconnect WiFi to save power (we only needed it for time sync)
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
    // debugPrint("WiFi disconnected - time synchronized");
  } else {
    debugPrint("WiFi connection failed! Time functions will not work correctly.");
  }

  // Initialize the web server (uses the same WiFi credentials or creates an AP if needed)
  taskServer.begin();

  debugPrint("Setup complete");

  // Initialize time check
  lastTimeCheck = millis();

  // Setup interrupts - ESP32 uses attachInterrupt differently
  attachInterrupt(digitalPinToInterrupt(DATA0_PIN), ISRreceiveData0, FALLING);
  attachInterrupt(digitalPinToInterrupt(DATA1_PIN), ISRreceiveData1, FALLING);

  // Test if RFID scanner is connected properly
  debugPrint("Testing RFID scanner connection...");

  // Give some time for the RFID module to initialize
  delay(500);

  // Check interrupt pins
  debugPrint("Checking interrupt pins...");
  Serial.print("DATA0_PIN (GPIO ");
  Serial.print(DATA0_PIN);
  Serial.print(") state: ");
  Serial.println(digitalRead(DATA0_PIN));

  Serial.print("DATA1_PIN (GPIO ");
  Serial.print(DATA1_PIN);
  Serial.print(") state: ");
  Serial.println(digitalRead(DATA1_PIN));

  // Check button
  Serial.print("STEPPER_BUTTON_PIN (GPIO ");
  Serial.print(STEPPER_BUTTON_PIN);
  Serial.print(") state: ");
  Serial.println(digitalRead(STEPPER_BUTTON_PIN));

  Serial.print("LED_BUTTON_PIN (GPIO ");
  Serial.print(LED_BUTTON_PIN);
  Serial.print(") state: ");
  Serial.println(digitalRead(LED_BUTTON_PIN));

  debugPrint("Ready to scan RFID cards");
  debugPrint("Connection: Grove -> Arduino Nano ESP32");
  debugPrint("Red -> 5V");
  debugPrint("Black -> GND");
  debugPrint("Yellow -> D0 (RX0)");
  debugPrint("White -> D1 (TX0)");
  debugPrint("Servo -> D5");
  debugPrint("LED -> A5");
  debugPrint("LED Button -> D3");
  debugPrint("Stepper -> D8, D9, D10, D11");
  debugPrint("Stepper ENA -> D6, ENB -> D7");
  debugPrint("RFID connected to: RX0 on GPIO0 (D0), TX0 on GPIO1 (D1)");
  debugPrint("Debug output on Serial1");

  // List scheduled tasks
  debugPrint("Scheduled Tasks:");
  for (int i = 0; i < MAX_SCHEDULED_TASKS; i++) {
    if (scheduledTasks[i].name == NULL) break;

    char taskInfo[100];
    sprintf(taskInfo, "- %s: [%d %d %d %d %d]",
            scheduledTasks[i].name,
            scheduledTasks[i].minute,
            scheduledTasks[i].hour,
            scheduledTasks[i].dayOfMonth,
            scheduledTasks[i].month,
            scheduledTasks[i].dayOfWeek);
    debugPrint(taskInfo);
  }

  Serial.println("IMPORTANT: If no interrupts trigger when scanning, try swapping D0/D1");
}

void loop() {
  // Get current time for various timers
  currentTime = millis();

  // Handle web server clients
  if (taskServer.isRunning()) {
    taskServer.handleClient();
  }

  // Check button state periodically
  checkStepperButton();
  checkServoButton();

  // Debug output for troubleshooting card reading
  if (DEBUG && recvBitCount > 0 && recvBitCount % 8 == 0 && !isCardReadOver) {
    static int lastPrintedBitCount = 0;
    if (lastPrintedBitCount != recvBitCount) {
      Serial.print("Bits received: ");
      Serial.print(recvBitCount);
      Serial.print(" - Current data: ");
      for (int i = 3; i >= 0; i--) {
        Serial.print(RFIDcardNum[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      lastPrintedBitCount = recvBitCount;
    }
  }

  // Handle stepper motor rotation
  if (buttonControlActive) {
    // If button is controlling the stepper, keep it running
    stepper.run();
  } else if (stepperRotating) {
    // Continue running the stepper if a scheduled rotation is in progress
    if (stepper.distanceToGo() != 0) {
      stepper.run();
    } else {
      // Rotation completed
      stepperRotating = false;
      debugPrint("Stepper motor rotation complete");
      disableStepperMotor(); // Disable motor when rotation is complete
    }
  } else if (stepperScheduled && !stepperRotating) {
    // Start the scheduled rotation
    startStepperRotation();
    stepperScheduled = false;
    stepperRotating = true;
  }

  // Check scheduled tasks periodically
  if (currentTime - lastTimeCheck >= TIME_CHECK_INTERVAL) {
    lastTimeCheck = currentTime;
    checkScheduledTasks();
  }

  // Check if any interrupt activity happened recently
  if (recvBitCount > 0 && !anyInterruptTriggered) {
    anyInterruptTriggered = true;
    Serial.println("*** RFID activity detected! ***");
    Serial.print("Current bit count: ");
    Serial.println(recvBitCount);
    lastInterruptTime = currentTime;
    tagLastSeen = currentTime;

    // Activate the servo if tag wasn't already present
    if (!tagPresent && !servoButtonPressed) {  // Don't override button control
      tagPresent = true;
      myServo.write(SERVO_OPEN_POS);
      Serial.println("Tag detected - Servo opening");
    }
  }

  // Give a little time for other processes
  delay(1);

  // Update tag present status based on recent activity
  if (tagPresent && (currentTime - tagLastSeen > TAG_TIMEOUT) && !servoButtonPressed) {
    // Only close the servo if the button isn't being pressed
    tagPresent = false;
    myServo.write(SERVO_CLOSED_POS);
    Serial.println("Tag removed - Servo closing");
  }

  // Reset the activity flag after 2 seconds of no activity
  if (anyInterruptTriggered && (currentTime - lastInterruptTime > 2000)) {
    Serial.println("RFID activity stopped - resetting");
    resetData();
    anyInterruptTriggered = false;
  }

  // Read card number bit
  if (isData0Low || isData1Low) {
    lastInterruptTime = currentTime; // Update last activity time
    tagLastSeen = currentTime;       // Update tag presence time

    if (1 == recvBitCount) { // even bit
      evenBit = (1-isData0Low) & isData1Low;
      if (DEBUG) {
        Serial.print("Even bit: ");
        Serial.println(evenBit);
      }
    }
    else if (recvBitCount >= 26) { // odd bit
      oddBit = (1-isData0Low) & isData1Low;
      isCardReadOver = 1;
      if (DEBUG) {
        Serial.print("Odd bit: ");
        Serial.println(oddBit);
        Serial.println("Card reading complete");
      }
      delay(10);
    }
    else {
      // Only if isData1Low = 1, card bit could be 1
      RFIDcardNum[2-(recvBitCount-2)/8] |= (isData1Low << (7-(recvBitCount-2)%8));

      if (DEBUG && (recvBitCount % 8 == 0)) {
        Serial.print("Received ");
        Serial.print(recvBitCount);
        Serial.println(" bits");
      }
    }
    // Reset data0 and data1
    isData0Low = 0;
    isData1Low = 0;
  }

  // Print the card id number
  if (isCardReadOver) {
    // Visual feedback - blink LED when card is read
    // We'll avoid turning on/off the LED here if the button is controlling it
    if (!servoButtonPressed) {
      digitalWrite(LED_PIN, HIGH);
    }

    // Always print raw data received, regardless of parity check
    Serial.println("==========================================");
    Serial.print("CARD READ COMPLETE - Received bits: ");
    Serial.println(recvBitCount);
    Serial.print("Raw data (HEX): ");
    Serial.print(RFIDcardNum[2], HEX);
    Serial.print(" ");
    Serial.print(RFIDcardNum[1], HEX);
    Serial.print(" ");
    Serial.print(RFIDcardNum[0], HEX);
    Serial.print(" ");
    Serial.println(RFIDcardNum[3], HEX);

    if (checkParity()) {
      // Current time for tracking read intervals
      unsigned long currentTime = millis();

      Serial.println("** VALID CARD DETECTED! **");

      // Update tag presence status and operate servo
      tagPresent = true;
      tagLastSeen = currentTime;

      // Only operate the servo if the button isn't already controlling it
      if (!servoButtonPressed) {
        myServo.write(SERVO_OPEN_POS);
      }

      // Print the card ID in multiple formats
      Serial.print("Card ID (DEC): ");
      unsigned long cardID = ((unsigned long)RFIDcardNum[2] << 16) |
                           ((unsigned long)RFIDcardNum[1] << 8) |
                            RFIDcardNum[0];
      Serial.println(cardID);

      // Print card ID in hexadecimal format
      Serial.print("Card ID (HEX): ");
      for (int i = 2; i >= 0; i--) {
        if (RFIDcardNum[i] < 0x10) Serial.print("0"); // Add leading zero if needed
        Serial.print(RFIDcardNum[i], HEX);
      }
      Serial.println();

      // Print formatted facility code and card number (Wiegand 26-bit format)
      unsigned int facilityCode = ((RFIDcardNum[2] & 0x01) << 7) | ((RFIDcardNum[1] & 0xF0) >> 1) | ((RFIDcardNum[1] & 0x08) >> 3);
      unsigned int cardNumber = ((RFIDcardNum[1] & 0x07) << 13) | (RFIDcardNum[0] << 5) | ((RFIDcardNum[3] & 0xF8) >> 3);

      Serial.print("Facility Code: ");
      Serial.println(facilityCode);
      Serial.print("Card Number: ");
      Serial.println(cardNumber);

      // Calculate and display time since last read
      if (lastReadTime > 0) {
        Serial.print("Time since last read: ");
        Serial.print((currentTime - lastReadTime) / 1000.0);
        Serial.println(" seconds");
      }
      lastReadTime = currentTime;
    } else {
      Serial.println("ERROR: Parity check failed or incomplete read!");

      // Still operate the servo for partial reads as the tag has been detected
      // Only if the button isn't already controlling it
      if (!servoButtonPressed) {
        tagPresent = true;
        tagLastSeen = currentTime;
        myServo.write(SERVO_OPEN_POS);
      }
    }
    Serial.println("==========================================");

    // Turn off the LED if we turned it on (and button isn't controlling it)
    if (!servoButtonPressed) {
      digitalWrite(LED_PIN, LOW);
    }
    resetData();
  }
}