#ifndef STEPPER_CONTROL_H
#define STEPPER_CONTROL_H

#include <Arduino.h>
#include <ESP32Servo.h>      // Include ESP32Servo library instead of Servo.h
#include <AccelStepper.h>    // Include the AccelStepper library

#define DEBUG 1              // Set to 1 to enable debug messages, 0 to disable
#define DATA0_PIN 0          // Data0 pin (RX0 on D0)
#define DATA1_PIN 1          // Data1 pin (TX0 on D1)
#define SERVO_PIN 4          // Servo motor connected to D4
#define STEPPER_BUTTON_PIN 2 // Button connected to D2
#define LED_BUTTON_PIN 3     // Button connected to D3

// Stepper motor pins
#define STEPPER_PIN1 8       // NEMA 17 stepper motor pin IN1
#define STEPPER_PIN2 9       // NEMA 17 stepper motor pin IN2
#define STEPPER_PIN3 10      // NEMA 17 stepper motor pin IN3
#define STEPPER_PIN4 11      // NEMA 17 stepper motor pin IN4
// L298N enable pins
#define STEPPER_ENA 6        // ENA pin for L298N driver
#define STEPPER_ENB 7        // ENB pin for L298N driver

// Cron-like scheduler structure
#define MAX_SCHEDULED_TASKS 10  // Maximum number of scheduled tasks
typedef struct {
    int minute;      // -1 for any value (wildcard), or 0-59
    int hour;        // -1 for any value (wildcard), or 0-23
    int dayOfMonth;  // -1 for any value (wildcard), or 1-31
    int month;       // -1 for any value (wildcard), or 1-12
    int dayOfWeek;   // -1 for any value (wildcard), or 0-6 (0 = Sunday)
    bool lastRun;    // Did this task run in the current period?
    const char* name; // Name of the task
} ScheduledTask;

extern ScheduledTask scheduledTasks[MAX_SCHEDULED_TASKS];

// Time tracking
extern unsigned long lastTimeCheck;
extern const unsigned long TIME_CHECK_INTERVAL;

// Stepper control
extern AccelStepper stepper;

// WiFi and time configuration
extern const char* ssid;
extern const char* password;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;

// Stepper control variables
extern const int STEPS_PER_REVOLUTION;
extern boolean stepperRotating;
extern boolean stepperScheduled;
extern boolean stepperButtonPressed;
extern boolean buttonControlActive;

// Button debounce variables
extern boolean lastStepperButtonState;
extern unsigned long lastDebounceTime;
extern unsigned long debounceDelay;

// Additional time tracking
extern unsigned long lastButtonCheck;
extern const unsigned long BUTTON_CHECK_INTERVAL;

// Function declarations
void checkScheduledTasks();
void getCurrentTime(int &minute, int &hour, int &dayOfMonth, int &month, int &dayOfWeek);
String getTimeString();
bool shouldRunTask(ScheduledTask* task, int minute, int hour, int dayOfMonth, int month, int dayOfWeek);
void scheduleStepperRotation(const char* taskName);
void startStepperRotation();
void checkStepperButton();
void enableStepperMotor();
void disableStepperMotor();

#endif //STEPPER_CONTROL_H