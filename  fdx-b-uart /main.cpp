#include <ESP32Servo.h>
#include <AccelStepper.h>
#include <WiFi.h>
#include "state.h"
#include "stepper_control.h"
#include "rfid_control.h"
#include "web_server.h"

// WiFi configuration
const char *webServerSSID = "";
const char *webServerPassword = "";

TaskSchedulerWebServer taskServer(webServerSSID, webServerPassword);

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
    Serial1.begin(9600); // Use UART1 for debug output

    // Redefine Serial to use Serial1 for all debug output
    #define Serial Serial1

    // Initialize RFID reader
    setupRFID();

    // Setup stepper motor enable pins
    pinMode(STEPPER_BUTTON_PIN, INPUT_PULLUP); // Button with pull-up
    pinMode(STEPPER_ENA, OUTPUT);
    pinMode(STEPPER_ENB, OUTPUT);
    digitalWrite(STEPPER_ENA, LOW);
    digitalWrite(STEPPER_ENB, LOW);

    // Configure stepper motor
    stepper.setMaxSpeed(500);
    stepper.setAcceleration(200);
    stepper.setSpeed(200);

    // Test stepper motor (small movement)
    debugPrint("Testing stepper motor...");
    enableStepperMotor();
    stepper.move(20);
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }
    delay(500);
    stepper.move(-20);
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }
    delay(500);
    disableStepperMotor();

    // Initialize WiFi and time
    debugPrint("Connecting to WiFi...");
    WiFi.begin(webServerSSID, webServerPassword);

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

    // Initialize the web server
    taskServer.begin();

    debugPrint("Setup complete");

    // Initialize time check
    lastTimeCheck = millis();

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
    debugPrint("Connection: EATAD425 RFID -> Arduino Nano ESP32");
    debugPrint("Red -> 5V");
    debugPrint("Black -> GND");
    debugPrint("TX (from module) -> RX0 on pin 0");
    debugPrint("Servo -> D4");
    debugPrint("LED -> A5");
    debugPrint("LED Button -> D3");
    debugPrint("Stepper -> D8, D9, D10, D11");
    debugPrint("Stepper ENA -> D6, ENB -> D7");
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

    // Process any incoming RFID data
    processRFIDData();

    // Handle stepper motor rotation
    if (buttonControlActive) {
        // If button is controlling the stepper, keep it running
        performSafeModeRotation(0, true);
    }
    //TODO: might have broken "every minute" task reset
    /*else if (stepperRotating) {
    // Continue running the stepper if a scheduled rotation is in progress
    if (stepper.distanceToGo() != 0) {
      stepper.run();
    } else {
      // Rotation completed
      stepperRotating = false;
      debugPrint("Stepper motor rotation complete");
      disableStepperMotor(); // Disable motor when rotation is complete
    }
  }*/ else if (stepperScheduled && !stepperRotating) {
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

    delay(1000);
}
