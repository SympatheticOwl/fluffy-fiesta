#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include <ESP32Servo.h>      // Include ESP32Servo library instead of Servo.h
#include <AccelStepper.h>    // Include the AccelStepper library

#define DEBUG 1              // Set to 1 to enable debug messages, 0 to disable
#define DATA0_PIN 0          // Data0 pin (RX0 on D0)
#define DATA1_PIN 1          // Data1 pin (TX0 on D1)
#define SERVO_PIN 4          // Servo motor connected to D4
#define LED_BUTTON_PIN 3     // Servo Button connected to D3
#define LED_PIN A5           // LED connected to A5

// Stepper motor pins
#define STEPPER_PIN1 8       // NEMA 17 stepper motor pin IN1
#define STEPPER_PIN2 9       // NEMA 17 stepper motor pin IN2
#define STEPPER_PIN3 10      // NEMA 17 stepper motor pin IN3
#define STEPPER_PIN4 11      // NEMA 17 stepper motor pin IN4
#define STEPPER_ENA 6        // ENA pin for L298N driver
#define STEPPER_ENB 7        // ENB pin for L298N driver
#define STEPPER_BUTTON_PIN 2 // Stepper Button connected to D2

// Function declarations
void debugPrint(const char* message);
void debugPrintHex(const char* prefix, byte value);

#endif //STATE_H