# Native interactive emulator

A desktop Win32 window that renders the live **480x800 1-bit e-ink framebuffer**
and drives **real activity code** from `src/activities/` through the native mock
hardware layer (`test/mocks/*.h`). Keyboard input is mapped to the device's
physical buttons, so you can navigate the actual UI on your PC and see it update
live — no hardware, no flashing.

This is different from the static screen-preview tool (`test/test_preview/`,
`pio test -e native -f test_preview`), which only dumps one-shot BMPs from
hand-copied render snippets. The emulator runs the genuine `Activity` objects
and their `loop()` / `render()` methods in an interactive event loop.

## Build & run

Requirements: PlatformIO + a plain MinGW g++ on `PATH` (no SDL2 / no extra deps).

```sh
# g++ must be on PATH (winlibs UCRT build used during development):
export PATH="/c/Users/leosa/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH"

pio run -e emulator
./.pio/build/emulator/program.exe
```

Headless verification (no window; dumps `emulator_selftest_*.bmp` and asserts the
framebuffer changes on input):

```sh
./.pio/build/emulator/program.exe --selftest
```

> Note: this repo is developed in a git worktree. The emulator lives entirely
> under `src/emulator/` + `test/mocks/`, so it works from any checkout/worktree
> of the branch as long as the `open-x4-sdk` submodule is initialized
> (`git submodule update --init --recursive`) — though the emulator itself does
> not compile any submodule code.

## Key mapping

| Keyboard        | Device button              |
|-----------------|----------------------------|
| Arrow Up        | `Up`                       |
| Arrow Down      | `Down`                     |
| Arrow Left      | `Left`                     |
| Arrow Right     | `Right`                    |
| Enter           | `Confirm`                  |
| Esc / Backspace | `Back`                     |
| Page Up         | `PageBack` (side button)   |
| Page Down       | `PageForward` (side button)|
| P               | `Power`                    |
| Q               | Quit the emulator          |

Buttons are the real `MappedInputManager::Button` enum. On each keypress the
emulator calls `input.simulatePress(btn)` then `activity->loop()` — exactly how
the firmware polls input then runs the activity loop — and re-renders. A ~30fps
timer also calls `loop()` so time-based behaviour (e.g. the dice roll animation)
advances.

## What's wired up

- **`DiceRollerActivity`** (`src/activities/apps/DiceRollerActivity.cpp`) — fully
  interactive: Up/Down change die type, Left/Right change count, Enter rolls
  (animated) and shows the result, Back returns to select.

This is the *real* activity source compiled against mocks, not a copy.

**`ActivityManager` now has real stack semantics** (`test/mocks/ActivityManager.h`
+ the out-of-line `ActivityManager::loop()` / global `activityManager` singleton
defined at the bottom of `test/mocks/Activity.h`, once `Activity` is complete).
It mirrors `src/activities/ActivityManager.h`'s pending-action model
(push/pop/replace/goHome deferred to the next `loop()` call, so an activity
never destroys itself mid-call) minus FreeRTOS/RenderLock, since native builds
are single-threaded. Concretely, real navigation calls now do something:
`Activity::finish()` → `activityManager.popActivity()`, `startActivityForResult()`
→ `activityManager.pushActivity()`, `onGoHome()` → `activityManager.goHome()`.
`runSelfTest()` in `src/emulator/main.cpp` proves this end-to-end: it pushes a
second activity on top of DiceRoller, confirms it, and checks the pop resumes
DiceRoller in its exact prior state (`stackDepth()` back to 0, framebuffer hash
identical to before the push) — plus a Back-at-root case that reaches
`goHome()` without crashing.

`goHome()` is currently a stub: no `HomeActivity` is wired in, so it just clears
the stack down to a blank screen instead of showing the real home/apps menu.
Wiring `HomeActivity`/`AppsMenuActivity` in is the natural next step now that
the stack itself works — see the limitation below.

## What's NOT wired up (yet)

- **`HomeActivity` / `AppsMenuActivity`**: not a mock-fidelity problem this
  time — `ActivityManager` is ready. The blocker is `AppsMenuActivity.cpp`
  transitively `#include`s dozens of other activities (several wireless-tool
  ones among them), so wiring it means either mocking `RadioManager`/SD first
  or trimming which category tiles it pulls in for the emulator build.
- **Anything under `src/activities/util/`** (`ConfirmationActivity`,
  `KeyboardEntryActivity`, `FullScreenMessageActivity`, `BmpViewerActivity`):
  their headers `#include "../Activity.h"` as a literal relative path. The
  compiler resolves that straight to the real `src/activities/Activity.h`
  (which drags in FreeRTOS) *before* it ever considers the `-Isrc/emulator/shim`
  redirect — quote-include relative-to-current-file resolution wins over `-I`
  search order. The include-redirection trick this emulator relies on simply
  doesn't reach these files. Fixing it for real means either guarding
  `src/activities/Activity.h`/`ActivityManager.h` themselves with an
  `EMULATOR_BUILD` passthrough (touches firmware source, small but deliberate)
  or rewriting those includes to the path-qualified form everything else uses.
  Until then, `src/emulator/main.cpp` has a small `DemoConfirmActivity` — an
  emulator-only stand-in with the same push/confirm/finish shape — used to
  exercise the `ActivityManager` stack in `runSelfTest()`.
- Anything else pulling in `RadioManager` (WiFi/BLE), the SD card filesystem,
  fonts from flash, or PNG/JPEG decoders — those mocks are still no-op stubs.
  Good next candidates that are fully self-contained: other
  `src/activities/apps/` utilities and games that don't touch radio/SD/fonts.

## How the wiring works (architecture)

The trick is include-path redirection, all local to `[env:emulator]`; nothing in
`src/activities/` is modified.

- `platform = native`, `lib_ldf_mode = off` (so PlatformIO does **not** compile
  the real `lib/` libraries), and `build_src_filter` compiles only
  `src/emulator/*` plus the one wired activity `.cpp`.
- Include search order: `-Isrc/emulator/shim` → `-Itest/mocks` → `-Isrc`.
  - `src/emulator/shim/` redirects the firmware's path-qualified includes
    (`activities/Activity.h`, `util/ButtonNavigator.h`, `components/UITheme.h`,
    `I18n.h`) to the mock layer / emulator versions, so the real activity picks
    up the header-only mock `Activity` instead of the hardware-coupled real one.
  - `test/mocks/GfxRenderer.h` forwards to `BitmapRenderer.h` when
    `EMULATOR_BUILD` is defined, so draw calls hit a real 480x800 framebuffer.
    (Native unit tests don't define `EMULATOR_BUILD` and keep the no-op mock.)
  - `src/emulator/shim/emu_prelude.h` is force-included (`-include`) to provide
    ambient `millis()/delay()/String`.
- `src/emulator/main.cpp` owns the Win32 window (`CreateWindowEx` + GDI
  `SetDIBitsToDevice` blit + `WM_KEYDOWN`/`WM_TIMER` message loop). The whole
  file is guarded by `#ifdef EMULATOR_BUILD`, so device/ESP32 builds see an empty
  translation unit.
- `src/emulator/shim/components/UITheme.h` is a *drawing* version of the UITheme
  mock — it actually renders headers/button-hints so screens look real.

## How to wire up another activity

1. In `src/emulator/main.cpp`, include the activity header and change the single
   `makeActivity()` factory + `kActivityName`:

   ```cpp
   #include "activities/apps/MyActivity.h"
   static std::unique_ptr<Activity> makeActivity(GfxRenderer& r, MappedInputManager& in) {
     return std::unique_ptr<Activity>(new MyActivity(r, in));
   }
   static const char* kActivityName = "MyActivity";
   ```

2. In `platformio.ini` under `[env:emulator]`, add the activity's `.cpp` to
   `build_src_filter`:

   ```ini
   build_src_filter =
     -<*>
     +<emulator/>
     +<activities/apps/MyActivity.cpp>
   ```

3. `pio run -e emulator` and fix include errors: if the activity includes a
   header that resolves to a hardware/no-op mock you need to behave, add a small
   drawing/functional shim under `src/emulator/shim/` (it's searched first). If
   it needs `RadioManager` / SD / `ActivityManager` behaviour, extend the
   corresponding mock (ideally gated behind `EMULATOR_BUILD` so unit tests are
   unaffected).
