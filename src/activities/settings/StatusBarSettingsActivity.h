#pragma once
// Arduino.h must come first: it pulls in the HardwareSerial.h -> esp32-hal.h ->
// freertos/FreeRTOS.h chain, which sets up the SMP FreeRTOS config (portYIELD_CORE
// etc.) that ESP32-S3 (dual-core) needs before any raw freertos/*.h header below can
// be parsed standalone. It happened to work on the single-core ESP32-C3, which never
// exercises that guard.
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Reader status bar configuration activity
class StatusBarSettingsActivity final : public Activity {
 public:
  explicit StatusBarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StatusBarSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;

  int selectedIndex = 0;

  void handleSelection();
};
