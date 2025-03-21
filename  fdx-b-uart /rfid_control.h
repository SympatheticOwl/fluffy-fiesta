#ifndef RFID_CONTROL_H
#define RFID_CONTROL_H

#include <Arduino.h>
#include <ESP32Servo.h>      // Include ESP32Servo library instead of Servo.h

#define DATA1_PIN 1          // TXD pin from RFID reader connected to TX1 (D1)
#define SERVO_PIN 4          // Servo motor connected to D4
#define LED_BUTTON_PIN 3     // Button connected to D3

// FDX-B protocol constants
#define FDXB_TAG_LENGTH 15       // Length of FDX-B tag data in bytes
#define FDXB_BUFFER_SIZE 64      // Buffer size for incoming UART data

// RFID variables
extern byte tagBuffer[FDXB_BUFFER_SIZE];  // Buffer for incoming tag data
extern byte tagData[FDXB_TAG_LENGTH];     // Processed tag data
extern int tagBufferIndex;                // Current position in buffer
extern boolean validTagRead;              // Flag for valid tag detection
extern unsigned long lastReadTime;        // Time of last tag read

// For detecting activity even without proper card reading
extern unsigned long lastActivityTime;
extern boolean activityDetected;

// button control
extern boolean servoButtonPressed;
extern boolean lastButtonState;
extern boolean buttonState;
extern unsigned long lastButtonDebounceTime;
extern const unsigned long buttonDebounceDelay;

// Servo control
extern Servo myServo;
extern boolean tagPresent;
extern unsigned long tagLastSeen;
extern const int TAG_TIMEOUT;
extern const int SERVO_OPEN_POS;
extern const int SERVO_CLOSED_POS;

void processRFIDData();
boolean parseTagData();
void resetRFIDData();
void checkServoButton();

#endif //RFID_CONTROL_H