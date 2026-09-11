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

using Button = MappedInputManager::Button;

// ------------------------------------------------------------
// Activity factory — the ONE place to swap which BASE activity runs.
// ------------------------------------------------------------
static std::unique_ptr<Activity> makeActivity(GfxRenderer& r, MappedInputManager& in) {
  return std::unique_ptr<Activity>(new DiceRollerActivity(r, in));
}
static const char* kActivityName = "DiceRoller";

// ------------------------------------------------------------
// DemoConfirmActivity — emulator-only, NOT firmware source.
//
// Used solely to exercise the ActivityManager stack (pushActivity() /
// finish() / popActivity()) in runSelfTest() below. The real confirmation
// dialog (src/activities/util/ConfirmationActivity.*) can't be compiled
// here yet: it includes its base class as "../Activity.h" (a literal
// relative path), which the compiler resolves straight to the real
// src/activities/Activity.h — bypassing the shim redirect that lets
// path-qualified includes like "activities/Activity.h" pick up the mock.
// Every activity under src/activities/util/ has this same pattern.
// Fixing it for real means either guarding the real Activity.h/
// ActivityManager.h with an EMULATOR_BUILD passthrough, or rewriting
// those includes — both touch firmware source and are a deliberate
// follow-up, not something to do incidentally here. See docs/emulator.md.
// ------------------------------------------------------------
class DemoConfirmActivity final : public Activity {
 public:
  DemoConfirmActivity(GfxRenderer& r, MappedInputManager& in, std::string heading)
      : Activity("DemoConfirm", r, in), heading(std::move(heading)) {}

  void render(RenderLock&&) override {
    renderer.clearScreen();
    renderer.drawCenteredText(12, 360, heading.c_str(), true, 1);
    renderer.drawCenteredText(10, 400, "Right = confirm, Left = cancel");
  }

  void loop() override {
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      setResult(ActivityResult{});
      finish();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      ActivityResult r; r.isCancelled = true;
      setResult(std::move(r));
      finish();
    }
  }

 private:
  std::string heading;
};

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
  // ActivityManager stack demo: push a second activity (DemoConfirmActivity
  // — see its comment above for why it's a stand-in for the real
  // ConfirmationActivity) on top of DiceRoller via the exact
  // pushActivity()/finish() path real firmware code uses, confirm it, and
  // verify popActivity() correctly resumes DiceRoller with its RESULT-screen
  // state intact. This exercises the stack machinery added to
  // test/mocks/ActivityManager.h + test/mocks/Activity.h, not just a single
  // activity running in isolation.
  // --------------------------------------------------------------
  activityManager.pushActivity(std::unique_ptr<Activity>(
      new DemoConfirmActivity(emu.renderer, emu.input, "Clear results?")));
  activityManager.loop();  // applies the push, calls DemoConfirmActivity::onEnter()
  emu.render();
  uint64_t hConfirm = fbHash(emu.renderer);
  emu.renderer.saveBMP("emulator_selftest_3_confirm_pushed.bmp");
  printf("[selftest] pushed DemoConfirm hash=%016llx (stackDepth=%zu)\n",
         (unsigned long long)hConfirm, activityManager.stackDepth());

  // Right = Confirm in DemoConfirmActivity -> finish() -> popActivity().
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
  }
  return runGui();
}

#else  // !EMULATOR_BUILD — empty translation unit for device builds
#endif
