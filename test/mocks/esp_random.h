#pragma once
#include <cstdlib>
#include <cstdint>
#include <cstddef>
inline uint32_t esp_random() { return (uint32_t)rand(); }
inline void esp_fill_random(void* buf, size_t len) {
  uint8_t* p = static_cast<uint8_t*>(buf);
  for (size_t i = 0; i < len; i++) p[i] = (uint8_t)rand();
}
