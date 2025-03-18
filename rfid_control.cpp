#include "rfid_control.h"
#include "state.h"

// RFID variables
byte RFIDcardNum[4] = {0};
byte evenBit = 0;
byte oddBit = 0;
byte isData0Low = 0;
byte isData1Low = 0;
int recvBitCount = 0;
byte isCardReadOver = 0;
unsigned long lastReadTime = 0;

// For detecting activity even without proper card reading
unsigned long lastInterruptTime = 0;
unsigned long currentTime = 0;
boolean anyInterruptTriggered = false;

// button control
boolean servoButtonPressed = false;
boolean lastButtonState = HIGH;  // Assuming using pull-up resistor
boolean buttonState = HIGH;      // Current debounced button state
unsigned long lastButtonDebounceTime = 0;
const unsigned long buttonDebounceDelay = 50;  // 50ms debounce

// Servo control
Servo myServo;
boolean tagPresent = false;
unsigned long tagLastSeen = 0;
const int TAG_TIMEOUT = 2000;      // Time in ms to wait before considering tag removed
const int SERVO_OPEN_POS = 180;    // Position for servo when tag detected (0-180)
const int SERVO_CLOSED_POS = 0;    // Position for servo when no tag detected (0-180)

void IRAM_ATTR ISRreceiveData0() {
  recvBitCount++;
  isData0Low = 1;
  
  // Visual feedback on interrupt - but minimize delays in interrupt
  // digitalWrite(LED_PIN, HIGH);
  
  // Important: If we've received enough bits to complete a read, set the flag
  if (recvBitCount >= 26) {
    isCardReadOver = 1;
  }
}

// Handle interrupt1
void IRAM_ATTR ISRreceiveData1() {
  recvBitCount++;
  isData1Low = 1;
  
  // Important: If we've received enough bits to complete a read, set the flag
  if (recvBitCount >= 26) {
    isCardReadOver = 1;
  }
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
        // Open servo
        myServo.write(SERVO_OPEN_POS);
        // Turn on LED
        digitalWrite(LED_PIN, HIGH);
        debugPrint("Button pressed - Servo opening, LED on");
      } 
      // Button is released
      else {
        servoButtonPressed = false;
        // Close servo
        myServo.write(SERVO_CLOSED_POS);
        // Turn off LED
        digitalWrite(LED_PIN, LOW);
        debugPrint("Button released - Servo closing, LED off");
      }
    }
  }
  
  // Save current reading for next comparison
  lastButtonState = reading;
}

byte checkParity() {
  int i = 0;
  int evenCount = 0;
  int oddCount = 0;
  
  // Check even parity bits
  for (i = 0; i < 8; i++) {
    if (RFIDcardNum[2] & (0x80 >> i)) {
      evenCount++;
    }
  }
  for (i = 0; i < 4; i++) {
    if (RFIDcardNum[1] & (0x80 >> i)) {
      evenCount++;
    }
  }
  
  // Check odd parity bits
  for (i = 4; i < 8; i++) {
    if (RFIDcardNum[1] & (0x80 >> i)) {
      oddCount++;
    }
  }
  for (i = 0; i < 8; i++) {
    if (RFIDcardNum[0] & (0x80 >> i)) {
      oddCount++;
    }
  }
  
  if (DEBUG) {
    Serial.print("Even count: ");
    Serial.print(evenCount);
    Serial.print(", Odd count: ");
    Serial.println(oddCount);
  }
  
  if (evenCount % 2 == evenBit && oddCount % 2 != oddBit) {
    return 1;
  } else {
    return 0;
  }
}

void resetData() {
  RFIDcardNum[0] = 0;
  RFIDcardNum[1] = 0;
  RFIDcardNum[2] = 0;
  RFIDcardNum[3] = 0;
  evenBit = 0;
  oddBit = 0;
  recvBitCount = 0;
  isData0Low = 0;
  isData1Low = 0;
  isCardReadOver = 0;
}