#ifndef RFID_CONTROL_H
#define RFID_CONTROL_H

#include <Arduino.h>
#include <ESP32Servo.h>      // Include ESP32Servo library instead of Servo.h

#define DATA0_PIN 0          // Data0 pin (RX0 on D0)
#define DATA1_PIN 1          // Data1 pin (TX0 on D1)
#define SERVO_PIN 4          // Servo motor connected to D4
#define LED_BUTTON_PIN 3     // Button connected to D3

// #define LED_PWM_CHANNEL 0    // PWM channel for LED (0-15 on ESP32)
// #define LED_PWM_FREQ 5000    // PWM frequency in Hz
// #define LED_PWM_RESOLUTION 8 // 8-bit resolution (0-255)
// #define LED_BRIGHTNESS 255   // Maximum brightness (0-255)

// RFID variables
extern byte RFIDcardNum[4];
extern byte evenBit;
extern byte oddBit;
extern byte isData0Low;
extern byte isData1Low;
extern int recvBitCount;
extern byte isCardReadOver;
extern unsigned long lastReadTime;

// For detecting activity even without proper card reading
extern unsigned long lastInterruptTime;
extern unsigned long currentTime;
extern boolean anyInterruptTriggered;

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

void IRAM_ATTR ISRreceiveData0();
void IRAM_ATTR ISRreceiveData1();
byte checkParity();
void resetData();
void checkServoButton();

#endif //RFID_CONTROL_H