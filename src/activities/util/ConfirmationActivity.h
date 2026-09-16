#pragma once
#include <functional>
#include <string>

#include "../../fontIds.h"
#include "../Activity.h"

class ConfirmationActivity : public Activity {
 private:
  // Input data
  std::string heading;
  std::string body;

  const int margin = 20;
  const int spacing = 30;
  const int fontId = UI_10_FONT_ID;

  std::string safeHeading;
  std::string safeBody;
  int startY = 0;
  int lineHeight = 0;

  // Touch boards hide the physical button-hint bar entirely (see
  // BaseTheme::drawButtonHints), so this activity draws its own Cancel/Confirm
  // tap targets in its place. Shared between render() (drawing) and loop()
  // (hit-testing) so they never drift apart.
  static constexpr int touchBarHeight = 60;

 public:
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};