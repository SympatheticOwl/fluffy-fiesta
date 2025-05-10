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

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600; // -6 hours for CST
const int daylightOffset_sec = 3600; // 3600 seconds = 1 hour DST offset

// Stepper control variables
const int STEPS_PER_REVOLUTION = 1200; // Standard for NEMA 17 (1.8° per step)
boolean stepperRotating = false;
boolean stepperScheduled = false;
boolean stepperButtonPressed = false; // Track button state
boolean buttonControlActive = false; // Is button currently controlling stepper

// Button debounce variables
boolean lastStepperButtonState = HIGH; // Assume button uses pull-up, so HIGH = not pressed
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // Debounce time in milliseconds

// Additional time tracking
unsigned long lastButtonCheck = 0;
const unsigned long BUTTON_CHECK_INTERVAL = 50; // Check button every 50ms

ScheduledTask scheduledTasks[MAX_SCHEDULED_TASKS] = {
    // Format: {minute, hour, dayOfMonth, month, dayOfWeek, lastRun, name}
    // {-1, -1, -1, -1, -1, false, "Every Minute"},  // Run every minute of every day

    // Commented out other schedules - uncomment if needed
    {0, 7, -1, -1, -1, false, "Morning Feeding"}, // Every day at 7:00 AM
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
    tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Failed to obtain time";
    }
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    return String(timeStringBuff);
}

// Function to get current time components
void getCurrentTime(int &minute, int &hour, int &dayOfMonth, int &month, int &dayOfWeek) {
    tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
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
    month = timeinfo.tm_mon + 1; // tm_mon is 0-11, we need 1-12
    dayOfWeek = timeinfo.tm_wday; // tm_wday is 0-6 (0 = Sunday)
}

void scheduleStepperRotation(const char *taskName) {
    String timeStr = getTimeString();
    char message[150];
    sprintf(message, "Scheduling stepper motor rotation for task: %s at %s",
            taskName, timeStr.c_str());
    debugPrint(message);
}

void startStepperRotation() {
    String timeStr = getTimeString();
    debugPrint(("Starting stepper motor rotation at " + timeStr).c_str());
    performSafeModeRotation(STEPS_PER_REVOLUTION, false); // Move 200 steps (one full revolution)
}

void safeRotate(int &currentSteps, int totalSteps) {
    // Calculate steps for the forward and backward motion
    const int FORWARD_STEPS = 200;
    const int BACKWARD_STEPS = 50;
    const int NET_STEPS_PER_CYCLE = FORWARD_STEPS - BACKWARD_STEPS; // 2400 net steps

    // Set the speed for the safe mode operation
    // You can adjust this value to match your original delay timing
    stepper.setMaxSpeed(500); // Adjust based on your needs
    stepper.setAcceleration(200); // Slower acceleration for safe mode

    debugPrint("Safe mode: Moving forward...");
    // The drill spiral rotates opposite of what traditional forward is so without
    // reversing it food will get pulled inward
    stepper.move(-FORWARD_STEPS);

    // Run the stepper until it completes the forward movement
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }

    debugPrint("Safe mode: Moving backward...");
    stepper.move(BACKWARD_STEPS);

    // Run the stepper until it completes the backward movement
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }

    // Update the current step count with the net movement
    currentSteps += NET_STEPS_PER_CYCLE;

    // Debug output
    char message[100];
    sprintf(message, "Safe mode progress: %d/%d steps", currentSteps, totalSteps);
    debugPrint(message);
}

void performSafeModeRotation(int totalSteps, boolean allowInterrupt) {
    debugPrint("Starting safe mode rotation to avoid jams");

    // Enable the stepper motor
    enableStepperMotor();

    int currentSteps = 0;
    if (buttonControlActive && allowInterrupt) {
        debugPrint("Button activated and interrupt allowed");
        // call safe rotate indefinitely until button released
        while (buttonControlActive) {
            checkStepperButton();
            debugPrint("buttonControlActive: ");
            Serial.println(buttonControlActive);
            safeRotate(currentSteps, totalSteps);
        }
    } else {
        while (currentSteps < totalSteps) {
            safeRotate(currentSteps, totalSteps);
        }

        // If we've overshot the target, correct the position
        // if (currentSteps > totalSteps) {
        //     int overshoot = currentSteps - totalSteps;
        //     debugPrint("Correcting overshoot...");
        //     stepper.move(-overshoot);
        //
        //     while (stepper.distanceToGo() != 0) {
        //         stepper.run();
        //     }
        // }
    }

    stepperRotating = false;

    disableStepperMotor();
    debugPrint("Safe mode rotation complete");
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
    if (millis() - lastDebounceTime > debounceDelay) {
        // If the button state has changed
        if (reading != stepperButtonPressed) {
            stepperButtonPressed = reading;

            // Button is pressed (LOW when using INPUT_PULLUP)
            if (stepperButtonPressed == LOW) {
                debugPrint("Button pressed - starting continuous stepper rotation");
                buttonControlActive = true;
            } else {
                debugPrint("Button released - stopping stepper rotation");
                stepper.stop(); // Stop the stepper
                buttonControlActive = false;
                disableStepperMotor();
            }
        }
    }

    // Save the current button state for next comparison
    lastStepperButtonState = reading;
}

bool shouldRunTask(ScheduledTask *task, int minute, int hour, int dayOfMonth, int month, int dayOfWeek) {
    return (task->minute == -1 || task->minute == minute) &&
           (task->hour == -1 || task->hour == hour) &&
           (task->dayOfMonth == -1 || task->dayOfMonth == dayOfMonth) &&
           (task->month == -1 || task->month == month) &&
           (task->dayOfWeek == -1 || task->dayOfWeek == dayOfWeek);
}
