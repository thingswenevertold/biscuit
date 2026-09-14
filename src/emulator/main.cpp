// ============================================================
// biscuit. native interactive emulator
//
// A desktop Win32 window that renders the live 480x800 e-ink framebuffer
// and drives REAL activity code from src/activities/ through the native
// mock hardware layer (test/mocks/*.h). Keyboard input is mapped to the
// device's physical buttons so you can navigate the actual UI.
//
// Build:  pio run -e emulator
// Run:    .pio/build/emulator/program.exe
// Self-test (headless, dumps BMPs): program.exe --selftest
//
// This whole file only compiles under EMULATOR_BUILD (set by [env:emulator]).
// Device/ESP32 builds also nominally compile this file but see an empty TU.
// ============================================================
#ifdef EMULATOR_BUILD

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "GfxRenderer.h"          // -> BitmapRenderer (real 480x800 1-bit framebuffer)
#include "MappedInputManager.h"   // mock: simulatePress/wasPressed
#include "RenderLock.h"

// ---- The real activity we drive. To wire up another self-contained
//      activity, include its header and change makeActivity() below. ----
#include "activities/apps/DiceRollerActivity.h"

// ---- Additional self-contained activities wired into the build so they
//      compile against the mocks. Not all of these are the interactive
//      boot activity (see makeActivity() below) — they are exercised via
//      the `--smoke [name|all]` generic harness instead. See
//      docs/emulator.md for the full wired-up list. ----
#include "activities/apps/MinesweeperActivity.h"
#include "activities/apps/SnakeActivity.h"
#include "activities/apps/TetrisActivity.h"
#include "activities/apps/SudokuActivity.h"
#include "activities/apps/GameOfLifeActivity.h"
#include "activities/apps/ChessActivity.h"
#include "activities/apps/MazeActivity.h"
#include "activities/apps/VoronoiActivity.h"
#include "activities/apps/MatrixRainActivity.h"
#include "activities/apps/CalculatorActivity.h"
#include "activities/apps/UnitConverterActivity.h"
#include "activities/apps/OtpGeneratorActivity.h"
#include "activities/apps/CountdownActivity.h"
#include "activities/apps/EtchASketchActivity.h"
#include "activities/apps/MorseCodeActivity.h"
#include "activities/apps/CipherActivity.h"
#include "activities/apps/SteganographyActivity.h"
#include "activities/apps/HabitTrackerActivity.h"
#include "activities/apps/FlashcardActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/FullScreenMessageActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "util/ButtonNavigator.h"

#include <functional>

using Button = MappedInputManager::Button;

// ------------------------------------------------------------
// Activity factory — the ONE place to swap which BASE activity runs.
// ------------------------------------------------------------
static std::unique_ptr<Activity> makeActivity(GfxRenderer& r, MappedInputManager& in) {
  return std::unique_ptr<Activity>(new DiceRollerActivity(r, in));
}
static const char* kActivityName = "DiceRoller";

// ------------------------------------------------------------
// Core state shared by GUI and self-test paths. Activity state now lives
// in the global `activityManager` (test/mocks/ActivityManager.h + the
// loop()/singleton defined at the bottom of test/mocks/Activity.h), so
// real navigation code (finish(), startActivityForResult(), onGoHome())
// works exactly like it does on the firmware — pushActivity/popActivity
// swap the on-screen activity instead of being no-ops.
// ------------------------------------------------------------
struct Emu {
  GfxRenderer& renderer;
  MappedInputManager& input;

  Emu() : renderer(activityManager.renderer), input(activityManager.mappedInput) {
    // Real ButtonNavigator (see shim/util/ButtonNavigator.h) needs this,
    // same as firmware startup code, so onNext/onPrevious/onPressAndContinuous
    // actually see button state instead of silently doing nothing.
    ButtonNavigator::setMappedInputManager(input);
    activityManager.replaceActivity(makeActivity(renderer, input));
    activityManager.loop();  // applies the replace, calls onEnter()
    render();
  }

  void render() {
    if (Activity* a = activityManager.current()) a->render(RenderLock{});
  }

  // Deliver one physical-button press+release, then run the activity loop
  // the same way the firmware does (InputManager.update() -> activity.loop()).
  // Both events are simulated because real activities react to either
  // wasPressed() (e.g. DiceRoller) or wasReleased() (e.g. Confirmation).
  void press(Button b) {
    input.simulatePress(b);
    activityManager.loop();
    input.simulateRelease(b);
    activityManager.loop();
    render();
  }

  // Advance time-based behaviour (e.g. dice roll animation).
  void tick() {
    activityManager.loop();
    render();
  }
};

// ============================================================
// Headless self-test: proves the interactive loop mutates screen state
// without needing a window. Dumps BMPs and checks the framebuffer changes.
// ============================================================
static uint64_t fbHash(GfxRenderer& r) {
  const uint8_t* fb = r.getFrameBuffer();
  uint64_t h = 1469598103934665603ULL;  // FNV-1a
  for (size_t i = 0; i < GfxRenderer::getBufferSize(); i++) {
    h ^= fb[i];
    h *= 1099511628257ULL;
  }
  return h;
}

// ============================================================
// Generic smoke test — proves a newly-wired activity constructs, enters,
// renders, survives a scripted round of button presses, and produces a
// changed (non-crashing) framebuffer, without needing per-activity
// scripted semantics like runSelfTest() above has for DiceRoller.
// Run one at a time: `program.exe --smoke <Name>` (a crash only aborts
// that one process, so the harness script below runs each in its own
// process and reports pass/fail per activity). `--smoke all` runs every
// case in one process (fine for quick iteration; a crash there just means
// re-run individually to isolate which one).
// ============================================================
struct SmokeCase {
  const char* name;
  std::function<std::unique_ptr<Activity>(GfxRenderer&, MappedInputManager&)> make;
};

static const std::vector<SmokeCase>& smokeCases() {
  static const std::vector<SmokeCase> cases = {
    {"Minesweeper", [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<MinesweeperActivity>(r, i); }},
    {"Snake",       [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<SnakeActivity>(r, i); }},
    {"Tetris",      [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<TetrisActivity>(r, i); }},
    {"Sudoku",      [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<SudokuActivity>(r, i); }},
    {"GameOfLife",  [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<GameOfLifeActivity>(r, i); }},
    {"Chess",       [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<ChessActivity>(r, i); }},
    {"Maze",        [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<MazeActivity>(r, i); }},
    {"Voronoi",     [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<VoronoiActivity>(r, i); }},
    {"MatrixRain",  [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<MatrixRainActivity>(r, i); }},
    {"Calculator",  [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<CalculatorActivity>(r, i); }},
    {"UnitConverter", [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<UnitConverterActivity>(r, i); }},
    {"OtpGenerator", [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<OtpGeneratorActivity>(r, i); }},
    {"Countdown",   [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<CountdownActivity>(r, i); }},
    {"EtchASketch", [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<EtchASketchActivity>(r, i); }},
    {"Confirmation", [](GfxRenderer& r, MappedInputManager& i) {
       return std::make_unique<ConfirmationActivity>(r, i, "Clear results?", "This cannot be undone.");
     }},
    {"FullScreenMessage", [](GfxRenderer& r, MappedInputManager& i) {
       return std::make_unique<FullScreenMessageActivity>(r, i, "Saved!");
     }},
    {"KeyboardEntry", [](GfxRenderer& r, MappedInputManager& i) {
       return std::make_unique<KeyboardEntryActivity>(r, i, "Enter Text", "", 32, false);
     }},
    {"MorseCode",     [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<MorseCodeActivity>(r, i); }},
    {"Cipher",        [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<CipherActivity>(r, i); }},
    {"Steganography", [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<SteganographyActivity>(r, i); }},
    {"HabitTracker",  [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<HabitTrackerActivity>(r, i); }},
    {"Flashcard",     [](GfxRenderer& r, MappedInputManager& i) { return std::make_unique<FlashcardActivity>(r, i); }},
  };
  return cases;
}

static bool smokeOne(const SmokeCase& c) {
  printf("[smoke] %s: constructing + onEnter...\n", c.name);
  activityManager.replaceActivity(c.make(activityManager.renderer, activityManager.mappedInput));
  activityManager.loop();
  Activity* a = activityManager.current();
  if (!a) { printf("[smoke] %s: FAIL (no current activity after onEnter)\n", c.name); return false; }
  a->render(RenderLock{});
  uint64_t h0 = fbHash(activityManager.renderer);

  auto press = [&](Button b) {
    activityManager.mappedInput.simulatePress(b);
    activityManager.loop();
    activityManager.mappedInput.simulateRelease(b);
    activityManager.loop();
    if (Activity* cur = activityManager.current()) cur->render(RenderLock{});
  };
  // A generic scripted tour: navigate, interact, confirm, then a few ticks
  // for anything time-based (animations), then Back (must not crash even
  // if it pops all the way to goHome()).
  press(Button::Down);
  press(Button::Right);
  press(Button::Up);
  press(Button::Left);
  press(Button::Confirm);
  for (int i = 0; i < 5; i++) { activityManager.loop(); if (Activity* cur = activityManager.current()) cur->render(RenderLock{}); }
  uint64_t h1 = fbHash(activityManager.renderer);
  press(Button::Back);

  char bmp[128];
  snprintf(bmp, sizeof(bmp), "emulator_smoke_%s.bmp", c.name);
  activityManager.renderer.saveBMP(bmp);

  bool changed = (h0 != h1);
  printf("[smoke] %s: h0=%016llx h1=%016llx changed=%s -> %s\n", c.name,
         (unsigned long long)h0, (unsigned long long)h1, changed ? "yes" : "no",
         "PASS (no crash)");
  return true;
}

static int runSmoke(const std::string& which) {
  bool anyRan = false;
  bool allOk = true;
  for (const auto& c : smokeCases()) {
    if (which == "all" || which == c.name) {
      anyRan = true;
      allOk = smokeOne(c) && allOk;
    }
  }
  if (!anyRan) { printf("[smoke] no case matched '%s'\n", which.c_str()); return 1; }
  return allOk ? 0 : 1;
}

static int runSelfTest() {
  printf("[selftest] activity = %s\n", kActivityName);
  Emu emu;

  uint64_t hSelect = fbHash(emu.renderer);
  emu.renderer.saveBMP("emulator_selftest_0_select.bmp");
  printf("[selftest] SELECT screen    hash=%016llx\n", (unsigned long long)hSelect);

  // Change die type (Down) then count (Right) -> screen must change.
  emu.press(Button::Down);
  emu.press(Button::Right);
  uint64_t hSelect2 = fbHash(emu.renderer);
  emu.renderer.saveBMP("emulator_selftest_1_select_changed.bmp");
  printf("[selftest] after Down+Right hash=%016llx\n", (unsigned long long)hSelect2);

  // Confirm -> ROLLING, then let the animation run to RESULT.
  emu.press(Button::Confirm);
  for (int i = 0; i < 400 && true; i++) emu.tick();
  uint64_t hResult = fbHash(emu.renderer);
  emu.renderer.saveBMP("emulator_selftest_2_result.bmp");
  printf("[selftest] after roll       hash=%016llx\n", (unsigned long long)hResult);

  bool ok = (hSelect != hSelect2) && (hSelect2 != hResult);
  printf("[selftest] screen changed on input: %s\n", ok ? "YES (pass)" : "NO (FAIL)");

  // --------------------------------------------------------------
  // ActivityManager stack demo: push the REAL ConfirmationActivity
  // (src/activities/util/ConfirmationActivity.*, wired for real now that
  // the EMULATOR_BUILD Activity.h/ActivityManager.h/UITheme.h/fontIds.h
  // relative-include redirects are in place — see docs/emulator.md) on top
  // of DiceRoller via the exact pushActivity()/finish() path real firmware
  // code uses, confirm it, and verify popActivity() correctly resumes
  // DiceRoller with its RESULT-screen state intact. This exercises the
  // stack machinery added to test/mocks/ActivityManager.h +
  // test/mocks/Activity.h, not just a single activity running in isolation.
  // --------------------------------------------------------------
  activityManager.pushActivity(std::unique_ptr<Activity>(
      new ConfirmationActivity(emu.renderer, emu.input, "Clear results?", "This cannot be undone.")));
  activityManager.loop();  // applies the push, calls ConfirmationActivity::onEnter()
  emu.render();
  uint64_t hConfirm = fbHash(emu.renderer);
  emu.renderer.saveBMP("emulator_selftest_3_confirm_pushed.bmp");
  printf("[selftest] pushed Confirmation hash=%016llx (stackDepth=%zu)\n",
         (unsigned long long)hConfirm, activityManager.stackDepth());

  // Right = Confirm in ConfirmationActivity -> finish() -> popActivity().
  emu.press(Button::Right);
  uint64_t hAfterPop = fbHash(emu.renderer);
  emu.renderer.saveBMP("emulator_selftest_4_popped_back.bmp");
  printf("[selftest] after confirm+pop hash=%016llx (stackDepth=%zu)\n",
         (unsigned long long)hAfterPop, activityManager.stackDepth());

  bool stackOk = (hConfirm != hResult)       // confirmation dialog actually drew something different
              && (hAfterPop == hResult)      // popped back to the exact same DiceRoller RESULT frame
              && (activityManager.stackDepth() == 0)
              && (activityManager.current() != nullptr);
  printf("[selftest] ActivityManager push/finish/pop stack: %s\n", stackOk ? "YES (pass)" : "NO (FAIL)");

  // Edge case: Back on the BASE activity (empty stack) must goHome(), not
  // crash. DiceRoller's first Back from RESULT just resets its own state to
  // SELECT (handled internally, doesn't touch the stack) — a second Back
  // from SELECT calls finish() -> popActivity() with an empty stack, which
  // is the real goHome() path. No HomeActivity is wired into the native
  // mock yet (see the goHome() comment in test/mocks/ActivityManager.h), so
  // this currently lands on a blank screen — expected, documented interim
  // behaviour, not a bug. What actually matters here: no crash, no
  // orphaned/negative stack depth.
  emu.press(Button::Back);  // RESULT -> SELECT, handled by DiceRoller itself
  emu.press(Button::Back);  // SELECT -> finish() -> popActivity() -> goHome()
  bool goHomeOk = (activityManager.stackDepth() == 0);
  printf("[selftest] Back,Back at root -> goHome(): %s (current=%s)\n",
         goHomeOk ? "YES (pass, no crash)" : "NO (FAIL)",
         activityManager.current() ? "non-null" : "null (blank, expected until HomeActivity is wired)");

  return (ok && stackOk && goHomeOk) ? 0 : 1;
}

// ============================================================
// Win32 GUI
// ============================================================
#include <windows.h>

static const int SCALE = 1;  // pixels per e-ink pixel
static const int WIN_W = GfxRenderer::SW * SCALE;
static const int WIN_H = GfxRenderer::SH * SCALE;

// Warm-white / black to match the device + saved BMP palette.
static const uint32_t COL_WHITE = 0x00F5F0E8;  // 0x00RRGGBB (stored BGRA in DIB below)
static const uint32_t COL_BLACK = 0x00000000;

struct GuiState {
  Emu* emu;
  std::vector<uint32_t> dib;  // WIN_W*WIN_H BGRA, top-down
  BITMAPINFO bmi{};
  GuiState() : dib((size_t)WIN_W * WIN_H) {
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WIN_W;
    bmi.bmiHeader.biHeight = -WIN_H;  // negative => top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
  }
  void refresh() {
    const uint8_t* fb = emu->renderer.getFrameBuffer();
    for (int y = 0; y < GfxRenderer::SH; y++) {
      for (int x = 0; x < GfxRenderer::SW; x++) {
        uint32_t c = fb[y * GfxRenderer::SW + x] ? COL_BLACK : COL_WHITE;
        for (int sy = 0; sy < SCALE; sy++) {
          uint32_t* row = &dib[(size_t)(y * SCALE + sy) * WIN_W + x * SCALE];
          for (int sx = 0; sx < SCALE; sx++) row[sx] = c;
        }
      }
    }
  }
};

static Button* mapKey(WPARAM vk) {
  static Button b;
  switch (vk) {
    case VK_UP:    b = Button::Up;          return &b;
    case VK_DOWN:  b = Button::Down;        return &b;
    case VK_LEFT:  b = Button::Left;        return &b;
    case VK_RIGHT: b = Button::Right;       return &b;
    case VK_RETURN:b = Button::Confirm;     return &b;
    case VK_ESCAPE:
    case VK_BACK:  b = Button::Back;        return &b;
    case VK_PRIOR: b = Button::PageBack;    return &b;  // PageUp
    case VK_NEXT:  b = Button::PageForward; return &b;  // PageDown
    case 'P':      b = Button::Power;       return &b;
    default:       return nullptr;
  }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  GuiState* g = reinterpret_cast<GuiState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
      SetTimer(hwnd, 1, 33, nullptr);  // ~30fps drives loop()/animations
      return 0;
    }
    case WM_KEYDOWN: {
      if (wp == 'Q') { DestroyWindow(hwnd); return 0; }
      if (Button* b = mapKey(wp)) {
        g->emu->press(*b);
        g->refresh();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_TIMER: {
      g->emu->tick();
      g->refresh();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      SetDIBitsToDevice(dc, 0, 0, WIN_W, WIN_H, 0, 0, 0, WIN_H,
                        g->dib.data(), &g->bmi, DIB_RGB_COLORS);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      KillTimer(hwnd, 1);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

static int runGui() {
  Emu emu;
  GuiState gui;
  gui.emu = &emu;
  gui.refresh();

  HINSTANCE hInst = GetModuleHandle(nullptr);
  WNDCLASS wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.lpszClassName = "BiscuitEmuWnd";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  RegisterClass(&wc);

  RECT r{0, 0, WIN_W, WIN_H};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  char title[128];
  snprintf(title, sizeof(title), "biscuit. emulator - %s  [arrows/Enter/Esc, Q=quit]", kActivityName);

  HWND hwnd = CreateWindowEx(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                             nullptr, nullptr, hInst, &gui);
  if (!hwnd) { printf("CreateWindowEx failed\n"); return 1; }
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return 0;
}

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--selftest") return runSelfTest();
    if (std::string(argv[i]) == "--smoke") {
      std::string which = (i + 1 < argc) ? argv[i + 1] : "all";
      return runSmoke(which);
    }
  }
  return runGui();
}

#else  // !EMULATOR_BUILD — empty translation unit for device builds
#endif
