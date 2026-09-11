#pragma once
#include <memory>
#include <string>
#include <utility>
#include "ActivityManager.h"
#include "ActivityResult.h"
#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "RenderLock.h"

class Activity {
  friend class ActivityManager;

 protected:
  std::string name;
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  ActivityResultHandler resultHandler;
  ActivityResult result;

 public:
  explicit Activity(std::string name, GfxRenderer& r, MappedInputManager& m)
      : name(std::move(name)), renderer(r), mappedInput(m) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual void requestUpdate(bool = false);
  virtual void requestUpdateAndWait();
  virtual bool skipLoopDelay() { return false; }
  virtual bool preventAutoSleep() { return false; }
  virtual bool isReaderActivity() const { return false; }

  // Defined below, once ActivityManager::loop() and the global
  // `activityManager` singleton are available — these mirror the real
  // Activity.cpp, which just forwards to activityManager.
  void startActivityForResult(std::unique_ptr<Activity>&& activity, ActivityResultHandler handler);
  void setResult(ActivityResult&& r) { result = std::move(r); }
  void finish();
  void onGoHome();
  void onSelectBook(const std::string&) {}
};

// ============================================================
// Out-of-line definitions needing the complete Activity type: the
// ActivityManager stack machinery (loop()) and the global singleton.
// Kept here rather than in ActivityManager.h because ActivityManager only
// forward-declares Activity — see the comment there.
// ============================================================
inline ActivityManager activityManager;

inline void Activity::requestUpdate(bool immediate) { activityManager.requestUpdate(immediate); }
inline void Activity::requestUpdateAndWait() { activityManager.requestUpdateAndWait(); }
inline void Activity::finish() { activityManager.popActivity(); }
inline void Activity::onGoHome() { activityManager.goHome(); }
inline void Activity::startActivityForResult(std::unique_ptr<Activity>&& activity, ActivityResultHandler handler) {
  resultHandler = std::move(handler);
  activityManager.pushActivity(std::move(activity));
}

inline void ActivityManager::loop() {
  if (currentActivity) currentActivity->loop();

  while (pendingAction != PendingAction::None) {
    if (pendingAction == PendingAction::Pop) {
      pendingAction = PendingAction::None;
      ActivityResult poppedResult;
      if (currentActivity) {
        poppedResult = std::move(currentActivity->result);
        currentActivity->onExit();
        currentActivity.reset();
      }
      if (stackActivities.empty()) {
        goHome();  // real ActivityManager: popping the last activity goes home
        continue;
      }
      currentActivity = std::move(stackActivities.back());
      stackActivities.pop_back();
      if (currentActivity->resultHandler) {
        auto handler = std::move(currentActivity->resultHandler);
        currentActivity->resultHandler = nullptr;
        handler(poppedResult);
      }
      continue;
    }
    if (pendingAction == PendingAction::GoHome) {
      pendingAction = PendingAction::None;
      if (currentActivity) {
        currentActivity->onExit();
        currentActivity.reset();
      }
      while (!stackActivities.empty()) {
        stackActivities.back()->onExit();
        stackActivities.pop_back();
      }
      continue;  // no HomeActivity wired yet — see goHome() comment
    }
    // Push or Replace: pendingActivity holds the activity to switch to.
    if (pendingAction == PendingAction::Replace) {
      if (currentActivity) currentActivity->onExit();
      while (!stackActivities.empty()) {
        stackActivities.back()->onExit();
        stackActivities.pop_back();
      }
    } else if (pendingAction == PendingAction::Push && currentActivity) {
      // Pushed activities are parked, not exited — matches the real
      // ActivityManager (no onPause/onResume, but push doesn't onExit()).
      stackActivities.push_back(std::move(currentActivity));
    }
    pendingAction = PendingAction::None;
    currentActivity = std::move(pendingActivity);
    currentActivity->onEnter();
  }
}
