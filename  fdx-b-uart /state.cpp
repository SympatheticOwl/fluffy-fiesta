#include "state.h"

void debugPrint(const char* message) {
  if (DEBUG) {
    Serial.println(message);
  }
}

void debugPrintHex(const char* prefix, byte value) {
  if (DEBUG) {
    Serial.print(prefix);
    Serial.println(value, HEX);
  }
}
