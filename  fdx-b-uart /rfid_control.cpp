#include "rfid_control.h"
#include "state.h"

// RFID variables
byte tagBuffer[FDXB_BUFFER_SIZE] = {0};   // Buffer for incoming UART data
byte tagData[FDXB_TAG_LENGTH] = {0};      // Processed tag data
int tagBufferIndex = 0;                   // Current position in buffer
boolean validTagRead = false;             // Flag for valid tag detection
unsigned long lastReadTime = 0;

// For detecting activity even without proper card reading
unsigned long lastActivityTime = 0;
boolean activityDetected = false;

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

// Process incoming RFID data from UART
void processRFIDData() {
  // Check if data is available on the serial port connected to the RFID reader
  if (Serial1.available() > 0) {
    debugPrint("Data received from RFID reader");
    
    // Reset activity timer
    lastActivityTime = millis();
    activityDetected = true;
    
    // Read available bytes
    while (Serial1.available() > 0 && tagBufferIndex < FDXB_BUFFER_SIZE) {
      byte incomingByte = Serial1.read();
      tagBuffer[tagBufferIndex] = incomingByte;
      
      // Debug output for raw data
      char byteStr[20];
      sprintf(byteStr, "Byte %d: 0x%02X", tagBufferIndex, incomingByte);
      debugPrint(byteStr);
      
      tagBufferIndex++;
      
      // Visual debug - flash LED on activity
      digitalWrite(LED_PIN, HIGH);
      delay(5);  // Very short flash
      digitalWrite(LED_PIN, LOW);
      
      // If we have enough data, try to parse it
      if (tagBufferIndex >= FDXB_TAG_LENGTH) {
        debugPrint("Buffer has enough data, attempting to parse tag");
        // Try to parse the data we have so far
        if (parseTagData()) {
          // Valid tag detected!
          validTagRead = true;
          lastReadTime = millis();
          tagLastSeen = millis();
          
          // Display tag data for debugging
          char hexString[50];
          sprintf(hexString, "Tag ID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", 
                 tagData[0], tagData[1], tagData[2], tagData[3], tagData[4],
                 tagData[5], tagData[6], tagData[7], tagData[8], tagData[9]);
          debugPrint(hexString);
          
          // Open the servo if tag is valid
          tagPresent = true;
          myServo.write(SERVO_OPEN_POS);
          debugPrint("Valid tag - Servo opening");
          
          // Turn on LED for visual confirmation
          digitalWrite(LED_PIN, HIGH);
          
          // Clear the buffer for next read
          resetRFIDData();
        } else {
          debugPrint("Tag parsing failed, continuing to collect data");
        }
      }
    }
    
    // If buffer is getting full without finding a valid tag, reset it
    if (tagBufferIndex >= FDXB_BUFFER_SIZE - 1) {
      debugPrint("Buffer full, resetting RFID data");
      resetRFIDData();
    }
  }
  
  // Check if tag has timed out (no longer present)
  if (tagPresent && (millis() - tagLastSeen > TAG_TIMEOUT)) {
    tagPresent = false;
    debugPrint("Tag timeout detected");
    
    // Only close servo if button isn't pressed
    if (!servoButtonPressed) {
      myServo.write(SERVO_CLOSED_POS);
      digitalWrite(LED_PIN, LOW);
      debugPrint("Tag timeout - Servo closing, LED off");
    } else {
      debugPrint("Tag timeout - Button still pressed, keeping servo open");
    }
  }
  
  // Check for activity timeout (debugging)
  if (activityDetected && (millis() - lastActivityTime > 5000)) {
    activityDetected = false;
    debugPrint("RFID reader activity timeout - No data for 5 seconds");
  }
}

// Parse the tag data from the buffer
// Returns true if a valid tag is found
boolean parseTagData() {
  debugPrint("Attempting to parse tag data from buffer");
  
  // Search for header sequence in buffer
  // Note: The EM4305 FDX-B typically starts with a specific header pattern
  // This is a simplified example - you may need to adjust based on your reader's output format
  
  // Debug dump of current buffer contents
  debugPrint("Current buffer contents:");
  for (int d = 0; d < tagBufferIndex; d += 8) {
    char bufferPart[100];
    sprintf(bufferPart, "Bytes %d-%d: %02X %02X %02X %02X %02X %02X %02X %02X",
            d, d+7,
            d < tagBufferIndex ? tagBuffer[d] : 0,
            d+1 < tagBufferIndex ? tagBuffer[d+1] : 0,
            d+2 < tagBufferIndex ? tagBuffer[d+2] : 0,
            d+3 < tagBufferIndex ? tagBuffer[d+3] : 0,
            d+4 < tagBufferIndex ? tagBuffer[d+4] : 0,
            d+5 < tagBufferIndex ? tagBuffer[d+5] : 0,
            d+6 < tagBufferIndex ? tagBuffer[d+6] : 0,
            d+7 < tagBufferIndex ? tagBuffer[d+7] : 0);
    debugPrint(bufferPart);
  }
  
  for (int i = 0; i < tagBufferIndex - FDXB_TAG_LENGTH; i++) {
    // Look for potential start sequence (this will vary based on your reader's protocol)
    // Common start patterns include 0x02 (STX) or other specific sequences
    if (tagBuffer[i] == 0x02) {  // STX - Start of Text character
      debugPrint("Found potential start marker at position:");
      char posStr[20];
      sprintf(posStr, "Position: %d, Value: 0x%02X", i, tagBuffer[i]);
      debugPrint(posStr);
      
      // Check if we have a complete data frame
      if (i + FDXB_TAG_LENGTH < tagBufferIndex) {
        // Check if frame ends with proper terminator (often 0x03 ETX or CR/LF)
        if (tagBuffer[i + FDXB_TAG_LENGTH - 1] == 0x03) {  // ETX - End of Text character
          debugPrint("Found complete frame with proper start/end markers");
          
          // Copy the tag data (excluding header and terminator)
          for (int j = 0; j < FDXB_TAG_LENGTH - 2; j++) {
            tagData[j] = tagBuffer[i + j + 1];  // Skip header byte
          }
          
          debugPrint("Valid tag data extracted");
          return true;  // Valid tag found
        } else {
          char endByte[30];
          sprintf(endByte, "End byte incorrect: 0x%02X, expected 0x03", 
                  tagBuffer[i + FDXB_TAG_LENGTH - 1]);
          debugPrint(endByte);
        }
      } else {
        debugPrint("Buffer too short for complete frame");
      }
    }
  }
  
  debugPrint("No valid tag pattern found in buffer");
  return false;  // No valid tag pattern found
}

// Reset RFID data buffers
void resetRFIDData() {
  debugPrint("Resetting RFID data buffers");
  memset(tagBuffer, 0, FDXB_BUFFER_SIZE);
  tagBufferIndex = 0;
  validTagRead = false;
}

// Check button state with debounce - kept the same as original
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
        // Only close servo if no tag is present
        if (!tagPresent) {
          myServo.write(SERVO_CLOSED_POS);
          digitalWrite(LED_PIN, LOW);
          debugPrint("Button released - Servo closing, LED off");
        }
      }
    }
  }
  
  // Save current reading for next comparison
  lastButtonState = reading;
}