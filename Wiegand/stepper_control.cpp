#include "stepper_control.h"
#include "state.h"
#include <Arduino.h>
#include <time.h>

// Time tracking
unsigned long lastTimeCheck = 0;
const unsigned long TIME_CHECK_INTERVAL = 10000; // Check time every 10 seconds

// Stepper motor control with AccelStepper
// Define a stepper motor controlled by 4 digital pins (4-wire stepper)
AccelStepper stepper(AccelStepper::FULL4WIRE, STEPPER_PIN1, STEPPER_PIN4, STEPPER_PIN2, STEPPER_PIN3);

// Time control variables
const char* ssid = "";       // Replace with your WiFi SSID
const char* password = "";   // Replace with your WiFi password (you may want to change this)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600;       // -6 hours for CST
const int daylightOffset_sec = 3600;     // 3600 seconds = 1 hour DST offset

// Stepper control variables
const int STEPS_PER_REVOLUTION = 200;    // Standard for NEMA 17 (1.8° per step)
boolean stepperRotating = false;
boolean stepperScheduled = false;
boolean stepperButtonPressed = false;     // Track button state
boolean buttonControlActive = false; // Is button currently controlling stepper

// Button debounce variables
boolean lastStepperButtonState = HIGH;  // Assume button uses pull-up, so HIGH = not pressed
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // Debounce time in milliseconds

// Additional time tracking
unsigned long lastButtonCheck = 0;
const unsigned long BUTTON_CHECK_INTERVAL = 50; // Check button every 50ms

ScheduledTask scheduledTasks[MAX_SCHEDULED_TASKS] = {
  // Format: {minute, hour, dayOfMonth, month, dayOfWeek, lastRun, name}
  // {-1, -1, -1, -1, -1, false, "Every Minute"},  // Run every minute of every day

  // Commented out other schedules - uncomment if needed
  {0, 7, -1, -1, -1, false, "Morning Feeding"},  // Every day at 7:00 AM
  {0, 19, -1, -1, -1, false, "Evening Feeding"}, // Every day at 7:00 PM
  //{30, 12, -1, -1, -1, false, "Noon Feeding"},   // Every day at 12:30 PM

  // End with an empty task to mark the end of the array
  {-1, -1, -1, -1, -1, false, NULL}
};

void checkScheduledTasks() {
  int currentMinute, currentHour, currentDayOfMonth, currentMonth, currentDayOfWeek;
  getCurrentTime(currentMinute, currentHour, currentDayOfMonth, currentMonth, currentDayOfWeek);

  // Debug current time
  char timeStr[100];
  sprintf(timeStr, "Current time: %02d:%02d, Date: %02d/%02d, Day: %d",
          currentHour, currentMinute, currentMonth, currentDayOfMonth, currentDayOfWeek);
  debugPrint(timeStr);

  // Store current minute for detecting minute changes
  static int lastMinute = -1;

  // Check if the minute has changed
  if (lastMinute != currentMinute) {
    debugPrint("Minute changed - resetting task run flags");

    // Reset all lastRun flags when the minute changes
    for (int i = 0; i < MAX_SCHEDULED_TASKS; i++) {
      if (scheduledTasks[i].name == NULL) break;
      scheduledTasks[i].lastRun = false;
    }

    // Update lastMinute
    lastMinute = currentMinute;
  }

  // Check if any scheduled task should run now
  for (int i = 0; i < MAX_SCHEDULED_TASKS; i++) {
    // Stop when we reach the end of defined tasks
    if (scheduledTasks[i].name == NULL) break;

    // Check if this task should run at the current time
    if (shouldRunTask(&scheduledTasks[i], currentMinute, currentHour,
                      currentDayOfMonth, currentMonth, currentDayOfWeek)) {

      // Only run the task if it hasn't run in this minute yet
      if (!scheduledTasks[i].lastRun) {
        // Mark the task as run
        scheduledTasks[i].lastRun = true;

        // Log the task trigger
        char taskStr[100];
        sprintf(taskStr, "Triggered task: %s at %02d:%02d",
                scheduledTasks[i].name, currentHour, currentMinute);
        debugPrint(taskStr);

        // Schedule the stepper motor rotation
        scheduleStepperRotation(scheduledTasks[i].name);
        stepperScheduled = true;
      }
    }
  }
}

// Function to get formatted time string
String getTimeString() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Failed to obtain time";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// Function to get current time components
void getCurrentTime(int &minute, int &hour, int &dayOfMonth, int &month, int &dayOfWeek) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    minute = 0;
    hour = 0;
    dayOfMonth = 1;
    month = 1;
    dayOfWeek = 0;
    return;
  }
  minute = timeinfo.tm_min;
  hour = timeinfo.tm_hour;
  dayOfMonth = timeinfo.tm_mday;
  month = timeinfo.tm_mon + 1;     // tm_mon is 0-11, we need 1-12
  dayOfWeek = timeinfo.tm_wday;    // tm_wday is 0-6 (0 = Sunday)
}

// Schedule a 360-degree rotation of the stepper motor
void scheduleStepperRotation(const char* taskName) {
  String timeStr = getTimeString();
  char message[150];
  sprintf(message, "Scheduling stepper motor rotation for task: %s at %s",
          taskName, timeStr.c_str());
  debugPrint(message);
}

// Start the actual rotation
void startStepperRotation() {
  String timeStr = getTimeString();
  debugPrint(("Starting stepper motor 360-degree rotation at " + timeStr).c_str());
  enableStepperMotor(); // Enable motor before starting rotation
  stepper.move(STEPS_PER_REVOLUTION);  // Move 200 steps (one full revolution)
}

void enableStepperMotor() {
  digitalWrite(STEPPER_ENA, HIGH);
  digitalWrite(STEPPER_ENB, HIGH);
  debugPrint("Stepper motor enabled");
}

void disableStepperMotor() {
  digitalWrite(STEPPER_ENA, LOW);
  digitalWrite(STEPPER_ENB, LOW);
  debugPrint("Stepper motor disabled");
}

// Check button state with debounce
void checkStepperButton() {
  // Read the current button state
  int reading = digitalRead(STEPPER_BUTTON_PIN);

  // Check if the button state has changed
  if (reading != lastStepperButtonState) {
    // Reset the debounce timer
    lastDebounceTime = millis();
  }

  // Check if debounce delay has passed since the last button state change
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed
    if (reading != stepperButtonPressed) {
      stepperButtonPressed = reading;

      // Button is pressed (LOW when using INPUT_PULLUP)
      if (stepperButtonPressed == LOW) {
        debugPrint("Button pressed - starting continuous stepper rotation");
        enableStepperMotor();
        buttonControlActive = true;

        // Set the stepper to run continuously
        stepper.setSpeed(200);  // Speed in steps per second
        stepper.moveTo(10000);  // Large number to keep it moving for a while
      }
      // Button is released
      else {
        debugPrint("Button released - stopping stepper rotation");
        stepper.stop();  // Stop the stepper
        buttonControlActive = false;
        disableStepperMotor();
      }
    }
  }

  // Save the current button state for next comparison
  lastStepperButtonState = reading;
}

bool shouldRunTask(ScheduledTask* task, int minute, int hour, int dayOfMonth, int month, int dayOfWeek) {
  return (task->minute == -1 || task->minute == minute) &&
         (task->hour == -1 || task->hour == hour) &&
         (task->dayOfMonth == -1 || task->dayOfMonth == dayOfMonth) &&
         (task->month == -1 || task->month == month) &&
         (task->dayOfWeek == -1 || task->dayOfWeek == dayOfWeek);
}