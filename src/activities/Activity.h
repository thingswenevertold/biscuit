#pragma once
#ifdef EMULATOR_BUILD
// Native emulator / unit-test build only. This real header transitively pulls
// in FreeRTOS + hardware-coupled headers (ActivityManager.h, RenderLock.h,
// GfxRenderer.h). Activities under src/activities/util/ include this file via
// the literal relative path "../Activity.h", which the compiler resolves
// straight to this file BEFORE the src/emulator/shim redirect (-I search order
// loses to quote-include relative-to-current-file resolution). Redirecting here
// to the native mock makes those activities compile natively. Both this path
// and src/emulator/shim/activities/Activity.h converge on the same physical
// mock file, so its #pragma once dedups them.
//
// This branch is ENTIRELY INERT on device/ESP32 builds: EMULATOR_BUILD is
// defined only by [env:emulator] in platformio.ini. Native unit tests
// ([env:native], -DNATIVE_TEST) do NOT define EMULATOR_BUILD and never reach
// this file — they include test/mocks/Activity.h directly via -Itest/mocks.
#include "../../test/mocks/Activity.h"
#else
#include <Logging.h>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

#include "ActivityManager.h"  // for using the ActivityManager singleton
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
  explicit Activity(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput)
      : name(std::move(name)), renderer(renderer), mappedInput(mappedInput) {}
  virtual ~Activity() = default;
  virtual void onEnter();
  virtual void onExit();
  virtual void loop() {}

  virtual void render(RenderLock&&) {}

  // If immediate is true, the update will be triggered immediately.
  // Otherwise, it will be deferred until the end of the current loop iteration.
  virtual void requestUpdate(bool immediate = false);

  // Request an immediate render and block until it completes.
  virtual void requestUpdateAndWait();

  virtual bool skipLoopDelay() { return false; }
  virtual bool preventAutoSleep() { return false; }
  virtual bool isReaderActivity() const { return false; }

  // Start a new activity without destroying the current one
  // Note: requestUpdate() will be invoked automatically once resultHandler finishes
  void startActivityForResult(std::unique_ptr<Activity>&& activity, ActivityResultHandler resultHandler);

  // Set the result to be passed back to the previous activity when this activity finishes
  void setResult(ActivityResult&& result);

  // Finish this activity and return to the previous one on the stack (if any)
  void finish();

  // Convenience method to facilitate API transition to ActivityManager
  // TODO: remove this in near future
  void onGoHome();
  void onSelectBook(const std::string& path);
};
#endif  // EMULATOR_BUILD
