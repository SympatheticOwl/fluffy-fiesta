#include "rfid_control.h"
#include "state.h"

// RFID variables
char rfidBuffer[32] = {0}; // Buffer to store incoming RFID data
int rfidBufferIndex = 0; // Current position in buffer
boolean tagDetected = false; // Flag to indicate if tag was detected
unsigned long lastReadTime = 0;

// For detecting activity even without proper card reading
unsigned long lastInterruptTime = 0;
unsigned long currentTime = 0;
boolean anyInterruptTriggered = false;

// button control
boolean servoButtonPressed = false;
boolean lastButtonState = HIGH; // Assuming using pull-up resistor
boolean buttonState = HIGH; // Current debounced button state
unsigned long lastButtonDebounceTime = 0;
const unsigned long buttonDebounceDelay = 50; // 50ms debounce

// Servo control
Servo myServo;
boolean tagPresent = false;
unsigned long tagLastSeen = 0;
const int TAG_TIMEOUT = 5000; // Time in ms to wait before considering tag removed
const int SERVO_CLOSED_POS = 0; // Position for servo when tag detected (0-90)
const int SERVO_OPEN_POS = 90; // Position for servo when no tag detected (0-90)

int currentPosition = SERVO_OPEN_POS;

void smoothServoMove(int targetPosition, int fullRangeTime) {
    // fullRangeTime is the time in ms it would take to move through the entire 0-90 range

    // Calculate distance and proportion of full range
    int distance = abs(targetPosition - currentPosition);
    int direction = (targetPosition > currentPosition) ? 1 : -1;

    // Calculate proportional movement time based on distance relative to full range
    float proportionOfFullRange = distance / 90.0;
    int moveTime = proportionOfFullRange * fullRangeTime;

    // Ensure minimum movement time for very small movements
    if (moveTime < 20 && distance > 0) moveTime = 20;

    Serial.print("Moving from ");
    Serial.print(currentPosition);
    Serial.print(" to ");
    Serial.print(targetPosition);
    Serial.print(" (distance: ");
    Serial.print(distance);
    Serial.print(" degrees, time: ");
    Serial.print(moveTime);
    Serial.println("ms)");

    // Number of steps (adjust for smoothness)
    int steps = distance;
    if (steps == 0) return; // No movement needed

    // Time per step
    float timePerStep = moveTime / (float) steps;

    for (int i = 0; i <= steps; i++) {
        // Calculate position with easing function
        float t = i / (float) steps; // Normalized time (0.0 to 1.0)
        float easeFactor;

        // Ease in and out (accelerate, then decelerate)
        if (t < 0.5) {
            // Accelerate
            easeFactor = 2 * t * t;
        } else {
            // Decelerate
            t = t - 1;
            easeFactor = 1 - (2 * t * t);
        }

        // Calculate current position
        int stepPosition = currentPosition + (direction * distance * easeFactor);

        // Move servo
        myServo.write(stepPosition);

        // Wait appropriate time for next step
        delay(timePerStep);
    }

    // Ensure we end exactly at the target position
    myServo.write(targetPosition);

    // Update the current position
    currentPosition = targetPosition;
}

void setupRFID() {
    // For ESP32 Nano, let's use Serial2 for the RFID reader
    // Serial2.begin(9600);  // Standard baud rate for most RFID readers

    // TODO: ???
    Serial2.begin(9600, SERIAL_8N1, 0, 1); // Hardware UART for RFID module

    pinMode(SERVO_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT); // Set LED pin as output
    pinMode(LED_BUTTON_PIN, INPUT_PULLUP); // Button with pull-up

    // Turn off LED initially
    digitalWrite(LED_PIN, LOW);

    // Setup servo
    ESP32PWM::allocateTimer(0); // Allocate timer 0 for ESP32 servo
    myServo.setPeriodHertz(50); // Standard 50Hz servo
    myServo.attach(SERVO_PIN, 500, 2400); // Attach the servo with min/max pulse width
    myServo.write(SERVO_OPEN_POS); // Start in closed position
    // moveServoWithSpeed(0, 180, 100);
    // delay(1000);

    debugPrint("RFID reader initialized on Serial2");

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
    // myServo.write(SERVO_CLOSED_POS);
    // delay(500);
    // myServo.write(SERVO_OPEN_POS);
    // delay(500);
    smoothServoMove(SERVO_CLOSED_POS, 3000);
    delay(500);
    smoothServoMove(SERVO_OPEN_POS, 3000);
    delay(500);
}

boolean processRFIDData() {
    boolean newTagRead = false;

    // Check if data is available from RFID reader
    while (Serial2.available()) {
        // Read a byte
        char c = Serial2.read();

        // Record time of activity
        // lastInterruptTime = millis();
        // tagLastSeen = millis();
        // anyInterruptTriggered = true;

        // Add to buffer if not overflowing
        if (rfidBufferIndex < sizeof(rfidBuffer) - 1) {
            rfidBuffer[rfidBufferIndex++] = c;
        }

        // Check for end of transmission (typically \r or \n)
        if (c == '\r' || c == '\n') {
            // Null terminate the string
            rfidBuffer[rfidBufferIndex] = 0;

            // If we have data, process it
            if (rfidBufferIndex > 1) {
                // More than just a newline
                tagDetected = true;
                newTagRead = true;

                // Log the read
                debugPrint("==========================================");
                debugPrint("RFID Tag detected");
                Serial.print("Raw data: ");
                Serial.println(rfidBuffer);

                // Calculate time since last read
                currentTime = millis();
                // if (lastReadTime > 0) {
                //   Serial.print("Time since last read: ");
                //   Serial.print((currentTime - lastReadTime) / 1000.0);
                //   Serial.println(" seconds");
                // }
                lastReadTime = currentTime;

                // Visual feedback - turn on LED
                if (!servoButtonPressed) {
                    digitalWrite(LED_PIN, HIGH);

                    // Update tag presence status and operate servo
                    tagPresent = true;
                    // tagLastSeen = currentTime;
                    // myServo.write(SERVO_CLOSED_POS);
                    smoothServoMove(SERVO_CLOSED_POS, 2500);
                    delay(1000);
                }

                debugPrint("==========================================");
            }

            // Reset buffer for next read
            resetRFIDBuffer();
        }
    }

    // Check for activity timeout
    // if (anyInterruptTriggered && (currentTime - lastInterruptTime > 5000)) {
    //   debugPrint("RFID activity stopped - resetting");
    //   resetRFIDBuffer();
    //   anyInterruptTriggered = false;
    // }

    if (tagPresent && (currentTime - lastReadTime > TAG_TIMEOUT) && !servoButtonPressed) {
        // Only close the servo if the button isn't being pressed
        tagPresent = false;
        smoothServoMove(SERVO_OPEN_POS, 2500);
        debugPrint("Tag removed - Servo closing");
        // Update tag present status based on recent activity
        Serial.print("Time since last read: ");
        Serial.print((currentTime - lastReadTime) / 1000.0);
        Serial.println(" seconds");

        digitalWrite(LED_PIN, LOW);

        // // Turn off LED
        // if (!servoButtonPressed) {
        // }
    }

    return newTagRead;
}

void resetRFIDBuffer() {
    memset(rfidBuffer, 0, sizeof(rfidBuffer));
    rfidBufferIndex = 0;
}

// Check button state with debounce
void checkServoButton() {
    // Read current button state
    int reading = digitalRead(LED_BUTTON_PIN);

    // Check if button state has changed
    if (reading != lastButtonState) {
        // Reset debounce timer
        lastButtonDebounceTime = millis();
    }

    // If enough time has passed since last state change
    if ((millis() - lastButtonDebounceTime) > buttonDebounceDelay) {
        // If button state has actually changed
        if (reading != buttonState) {
            buttonState = reading;

            // Button is pressed (LOW when using INPUT_PULLUP)
            if (buttonState == LOW) {
                servoButtonPressed = true;
                // Turn on LED
                digitalWrite(LED_PIN, HIGH);

                // Open servo
                // myServo.write(SERVO_CLOSED_POS);
                smoothServoMove(SERVO_CLOSED_POS, 2500);
                delay(500);

                debugPrint("Button pressed - Servo opening, LED on");
            }
            // Button is released
            else {
                servoButtonPressed = false;
                // Turn off LED
                digitalWrite(LED_PIN, LOW);
                // Close servo
                // myServo.write(SERVO_OPEN_POS);
                smoothServoMove(SERVO_OPEN_POS, 2500);
                delay(500);

                debugPrint("Button released - Servo closing, LED off");
            }
        }
    }

    // Save current reading for next comparison
    lastButtonState = reading;
}
