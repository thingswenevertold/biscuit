#pragma once

// ------------------------------------------------------------
// EMULATOR_BUILD redirect (same pattern as src/activities/Activity.h /
// ActivityManager.h — see docs/emulator.md). A handful of firmware
// activities #include this file via a literal relative path
// ("../../components/UITheme.h" from src/activities/util/), which the
// compiler resolves straight to this real file, bypassing the
// -Isrc/emulator/shim redirect that lets path-qualified includes
// ("components/UITheme.h") pick up the drawing shim. Guarding here closes
// that gap for those relative-include call sites too. Inert on device
// builds: EMULATOR_BUILD is only defined by [env:emulator].
// ------------------------------------------------------------
#ifdef EMULATOR_BUILD
#include "../emulator/shim/components/UITheme.h"
#else

#include <functional>
#include <memory>

#include "CrossPointSettings.h"
#include "components/themes/BaseTheme.h"

class MappedInputManager;

class UITheme {
  // Static instance
  static UITheme instance;

 public:
  UITheme();
  static UITheme& getInstance() { return instance; }

  const ThemeMetrics& getMetrics() const { return *currentMetrics; }
  const BaseTheme& getTheme() const { return *currentTheme; }
  void reload();
  void setTheme(CrossPointSettings::UI_THEME type);
  static int getNumberOfItemsPerPage(const GfxRenderer& renderer, bool hasHeader, bool hasTabBar, bool hasButtonHints,
                                     bool hasSubtitle);

  // Touch companion to BaseTheme::drawList(): mirrors its exact row height /
  // pagination geometry (see BaseTheme.cpp) so a tap lands on the same row the
  // user sees highlighted. Callers must invoke this from loop(), never from
  // render() -- render() runs on its own FreeRTOS task (ActivityManagerRender)
  // concurrently with loop(), so mutating activity state (e.g. selectorIndex)
  // from render() would be an unsynchronized cross-task race. Returns the
  // absolute item index whose row was tapped-and-released this frame, or -1 if
  // none. `hasSubtitle` must match the rowSubtitle-vs-nullptr choice passed to
  // the corresponding drawList() call for this list.
  static int hitTestList(const MappedInputManager& mappedInput, Rect rect, int itemCount, int selectedIndex,
                         bool hasSubtitle);
  static std::string getCoverThumbPath(std::string coverBmpPath, int coverHeight);
  static UIIcon getFileIcon(const std::string& filename);
  static int getStatusBarHeight();
  static int getProgressBarHeight();

 private:
  const ThemeMetrics* currentMetrics;
  std::unique_ptr<BaseTheme> currentTheme;
};

// Helper macro to access current theme
#define GUI UITheme::getInstance().getTheme()

#endif  // EMULATOR_BUILD
