#pragma once
// Force-included (-include) for every emulator TU. Provides the ambient
// Arduino symbols (millis/delay/yield/String) that firmware code assumes are
// globally available, WITHOUT pulling test/mocks/Arduino.h (which also defines
// esp_random() and would clash with test/mocks/esp_random.h in the same TU).
#include <algorithm>
#include <cstdint>
#include <string>

using String = std::string;

// Monotonic clock for the emulator. Advances 10ms per call so that time-based
// activity logic (e.g. DiceRoller's roll animation) progresses as loop() runs.
inline unsigned long millis() {
  static unsigned long t = 0;
  return t += 10;
}
inline void delay(unsigned long) {}
inline void yield() {}
