#pragma once
#include <memory>
#include <string>
#include <vector>
#include "GfxRenderer.h"
#include "MappedInputManager.h"

class Activity;

// ============================================================
// biscuit. native test/emulator mock — ActivityManager
//
// Mirrors the real src/activities/ActivityManager.h stack/pending-action
// model (see that file for the canonical semantics + comments) but single
// threaded and without FreeRTOS/RenderLock, since native builds have no
// render task. loop() must be called by the harness (native test main /
// emulator main.cpp) after each simulated input, the same way the real
// firmware's main loop calls activityManager.loop() every iteration.
//
// pushActivity/popActivity/replaceActivity/goHome only record a pending
// action here (matching the real ActivityManager) — the actual stack
// mutation + onEnter/onExit calls happen inside loop(), defined out-of-line
// at the bottom of Activity.h once Activity is a complete type.
// ============================================================
class ActivityManager {
 public:
  GfxRenderer renderer;
  MappedInputManager mappedInput;
  ActivityManager(GfxRenderer& r, MappedInputManager& m) : renderer(r), mappedInput(m) {}
  ActivityManager() : renderer(defaultRenderer), mappedInput(defaultInput) {}

  Activity* current() const { return currentActivity.get(); }
  size_t stackDepth() const { return stackActivities.size(); }

  void pushActivity(std::unique_ptr<Activity>&& activity) {
    pendingActivity = std::move(activity);
    pendingAction = PendingAction::Push;
  }
  void popActivity() { pendingAction = PendingAction::Pop; }
  void replaceActivity(std::unique_ptr<Activity>&& activity) {
    pendingActivity = std::move(activity);
    pendingAction = PendingAction::Replace;
  }
  // Real firmware replaces with HomeActivity; no HomeActivity is wired into
  // the native mock yet (needs SD/network mocks first), so this just clears
  // down to an empty screen. Follow-up: wire HomeActivity and call
  // replaceActivity(<HomeActivity>) here instead.
  void goHome() { pendingAction = PendingAction::GoHome; }
  void goToReader(std::string) {}
  void goToFileBrowser(std::string = {}) {}
  void requestUpdate(bool = false) {}
  void requestUpdateAndWait() {}

  // Drive one iteration: run currentActivity->loop(), then apply whatever
  // navigation request it (or its onEnter) queued. Call this once per
  // simulated frame/input, same as the real firmware's main loop.
  void loop();

 private:
  enum class PendingAction { None, Push, Pop, Replace, GoHome };
  PendingAction pendingAction = PendingAction::None;
  std::unique_ptr<Activity> pendingActivity;
  std::vector<std::unique_ptr<Activity>> stackActivities;
  std::unique_ptr<Activity> currentActivity;
  GfxRenderer defaultRenderer;
  MappedInputManager defaultInput;
};

// Global singleton — defined (as a C++17 inline variable) at the bottom of
// Activity.h, once Activity is a complete type. Any code that includes
// Activity.h (directly, or via an activity header) can use it.
extern ActivityManager activityManager;
