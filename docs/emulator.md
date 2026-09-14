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

Generic per-activity smoke test (no window; constructs one activity, runs a
scripted button sequence, checks the process doesn't crash, dumps
`emulator_smoke_<Name>.bmp`). Useful when wiring up a new activity that
doesn't have (or need) bespoke `runSelfTest()` assertions:

```sh
./.pio/build/emulator/program.exe --smoke Minesweeper   # one case
./.pio/build/emulator/program.exe --smoke all           # every wired case, one process
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
  (animated) and shows the result, Back returns to select. This is still the
  one activity `makeActivity()` boots into the interactive Win32 window by
  default (see "How to wire up another activity" below to swap it).
- **22 more `src/activities/` files compile and run against the mocks**,
  verified via the `--smoke [name|all]` generic harness in
  `src/emulator/main.cpp` (constructs the activity, `onEnter()`s it, renders,
  runs a scripted Down/Right/Up/Left/Confirm + a few idle ticks, renders
  again, checks the process didn't crash, dumps `emulator_smoke_<Name>.bmp`).
  Games/utilities with no radio/SD/font/crypto dependency:
  `MinesweeperActivity`, `SnakeActivity`, `TetrisActivity`, `SudokuActivity`,
  `GameOfLifeActivity`, `ChessActivity`, `MazeActivity`, `VoronoiActivity`,
  `MatrixRainActivity`, `CalculatorActivity`, `UnitConverterActivity`,
  `OtpGeneratorActivity`, `CountdownActivity`, `EtchASketchActivity` (uses the
  no-op `HalStorage` mock — save/load silently fail, no crash). Plus
  `src/activities/util/ConfirmationActivity` and `FullScreenMessageActivity`
  (the interactive Confirmation flow now also drives `runSelfTest()`'s
  push/pop stack demo — see below), `src/activities/util/KeyboardEntryActivity`
  (now genuinely typeable — see the `ButtonNavigator` note below), and
  `MorseCodeActivity`, `CipherActivity`, `SteganographyActivity`,
  `HabitTrackerActivity`, `FlashcardActivity` (the latter three exercise the
  extended no-op `HalStorage`/`FsFile`/`HalFile` mock — file I/O silently
  no-ops, no crash, no persistence).
  A handful of these (`SnakeActivity`, `SudokuActivity`, `MatrixRainActivity`,
  `UnitConverterActivity`, `EtchASketchActivity`, `HabitTrackerActivity`,
  `FlashcardActivity`, `ConfirmationActivity`, `FullScreenMessageActivity`)
  show `changed=no` in the generic smoke script — that's a limitation of the
  generic scripted input (e.g. Snake only moves on a movement timer tick, not
  a single Confirm), not a bug; the saved BMPs confirm real (non-blank)
  content is drawn in every case.

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
- **`src/activities/util/` relative-include blocker — FIXED, and now wired.**
  Those headers (`ConfirmationActivity`, `KeyboardEntryActivity`,
  `FullScreenMessageActivity`, `BmpViewerActivity`) `#include "../Activity.h"`
  (and, transitively, `"../../components/UITheme.h"`, `"../../fontIds.h"`) as
  literal relative paths, so the compiler resolves them straight to the real
  firmware headers, bypassing the `-Isrc/emulator/shim` redirect that only
  intercepts path-qualified includes. Fixed at the source with the same
  `#ifdef EMULATOR_BUILD` → mock/shim redirect `#else` → real content `#endif`
  pattern on **`src/activities/Activity.h`**, **`src/activities/ActivityManager.h`**,
  **`src/components/UITheme.h`**, and **`src/fontIds.h`** (the last one avoids a
  macro-redefinition warning and keeps font-ID values consistent with what
  the mock `BitmapRenderer`'s text-scaling logic expects). All four `#else`
  branches are byte-for-byte the original file; `EMULATOR_BUILD` is only
  defined by `[env:emulator]`, and native unit tests never reach these files
  (confirmed: `pio test -e native` stays 47/47 green through every change in
  this doc). `ConfirmationActivity` and `FullScreenMessageActivity` are now
  wired for real — `src/emulator/main.cpp`'s `runSelfTest()` pushes the real
  `ConfirmationActivity` (not a stand-in) to exercise the `ActivityManager`
  stack. `KeyboardEntryActivity` is wired too (see the `ButtonNavigator` note
  below for why it's actually typeable). `BmpViewerActivity` is still
  deferred — it needs `<Bitmap.h>` (image decode), out of scope here.
- **`ButtonNavigator` — now the REAL implementation, not the no-op mock.**
  `src/emulator/shim/util/ButtonNavigator.h` used to redirect
  `#include "util/ButtonNavigator.h"` to `test/mocks/ButtonNavigator.h`, whose
  `onNext`/`onPrevious`/`onPressAndContinuous` were all empty — silently
  dropping movement input for every activity that uses them (most of the
  ones listed above, including all the games). The shim now redirects to the
  real `src/util/ButtonNavigator.h` instead (`src/util/ButtonNavigator.cpp`
  is compiled into `[env:emulator]`'s `build_src_filter`); it's pure logic
  with no hardware dependency beyond a `const MappedInputManager*`, which
  still resolves to the native mock via the normal `-Itest/mocks` search
  path. This required two small compatibility changes: (1)
  `test/mocks/MappedInputManager.h`'s `wasPressed`/`wasReleased`/`isPressed`/
  `getHeldTime` are now `const`-qualified (with the mutated edge-triggered
  flags marked `mutable`) to match the real hardware `MappedInputManager`'s
  signature, which `ButtonNavigator` requires; (2) `<algorithm>` (for
  `std::any_of`) is force-included via `emu_prelude.h`. `src/emulator/main.cpp`
  calls `ButtonNavigator::setMappedInputManager()` once at startup, same as
  real firmware init code. Native unit tests are unaffected (they don't use
  `ButtonNavigator` through real activity code, and `const`-qualifying mock
  methods doesn't break existing non-const call sites).
- **`HalStorage`/`FsFile`/`HalFile` mock extended** (still fully no-op — no
  real file I/O happens, every read/exists/open fails safely) to cover the
  wider API surface newer activities call: `seekSet`, `rename` (both on the
  file and on `Storage`), `isOpen`, `size`/`fileSize`, `read(void*, size_t)`/
  `write(const void*, size_t)`, and a `HalFile` alias for `FsFile` (mirroring
  the real `lib/hal/HalStorage.h`'s `using FsFile = HalFile;`). This unblocked
  `FlashcardActivity`, `SteganographyActivity`, `HabitTrackerActivity`.
- **`HomeActivity` / `AppsMenuActivity`**: not a mock-fidelity problem this
  time — `ActivityManager` is ready. The blocker is `AppsMenuActivity.cpp`
  transitively `#include`s dozens of other activities (several wireless-tool
  ones among them), so wiring it means either mocking `RadioManager`/SD first
  or trimming which category tiles it pulls in for the emulator build.
- Still blocked / deferred (checked, not attempted further):
  - `ClockActivity`, `EmergencyActivity` — pull in `<WiFi.h>`/`RadioManager`.
  - `QrGeneratorActivity`, `QrTotpActivity`, `MedicalCardActivity` — need
    `util/QrUtils.h`, which needs the real `<qrcode.h>` (bitbank2/QRCode) and
    `src/components/themes/BaseTheme.h`'s theme-rendering pipeline; not
    available under `lib_ldf_mode = off`.
  - `TotpActivity`, `QrTotpActivity` — also need `<mbedtls/md.h>` (HMAC-SHA1),
    not available in the native toolchain here.
  - `PasswordGeneratorActivity` — needs `stores/PasswordStore.h`, whose `.cpp`
    needs `<ArduinoJson.h>`; not wired into `[env:emulator]`'s `-I` paths.
  - `BmpViewerActivity` — needs `<Bitmap.h>` (image decode).
- Anything else pulling in `RadioManager` (WiFi/BLE), the SD card filesystem,
  fonts from flash, PNG/JPEG decoders, or `mbedtls` — those mocks are still
  no-op stubs or simply absent.

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
- **The `EMULATOR_BUILD` header guard** (real `src/activities/Activity.h` /
  `ActivityManager.h`) is the one deliberate touch of firmware source. It exists
  because the `-Isrc/emulator/shim` redirect cannot intercept quote-includes
  that use a relative path resolving to the real file (the `util/*`
  `"../Activity.h"` case). The guard is inert on device builds. When adding new
  firmware-source `#ifdef EMULATOR_BUILD` branches, keep them equally inert and
  re-run `pio test -e native` (which does *not* define `EMULATOR_BUILD`) plus,
  ideally, `pio run -e default` to confirm the device build still compiles.
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
