#ifndef RFID_CONTROL_H
#define RFID_CONTROL_H

#include <Arduino.h>
#include <ESP32Servo.h>      // Include ESP32Servo library instead of Servo.h

// RFID related defines
#define RFID_RX_PIN 16       // RX pin for Serial2 (GPIO16 on most ESP32 boards)
#define SERVO_PIN 4          // Servo motor connected to D4
#define LED_BUTTON_PIN 3     // Button connected to D3

// RFID variables
extern char rfidBuffer[32];   // Buffer to store incoming RFID data
extern int rfidBufferIndex;   // Current position in buffer
extern boolean tagDetected;   // Flag to indicate if tag was detected
extern unsigned long lastReadTime;

// For detecting activity even without proper card reading
extern unsigned long lastInterruptTime;
extern unsigned long currentTime;
extern boolean anyInterruptTriggered;
extern unsigned long tagLastSeen;

// button control
extern boolean servoButtonPressed;
extern boolean lastButtonState;
extern boolean buttonState;
extern unsigned long lastButtonDebounceTime;
extern const unsigned long buttonDebounceDelay;

// Servo control
extern Servo myServo;
extern boolean tagPresent;
extern const int TAG_TIMEOUT;
extern const int SERVO_OPEN_POS;
extern const int SERVO_CLOSED_POS;

void setupRFID();
boolean processRFIDData();
void resetRFIDBuffer();
void checkServoButton();

#endif //RFID_CONTROL_H