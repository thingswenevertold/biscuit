#include "MappedInputManager.h"

#include <BoardConfig.h>
#include <GfxRenderer.h>

#include "CrossPointSettings.h"

namespace {
using ButtonIndex = uint8_t;

struct SideLayoutMap {
  ButtonIndex pageBack;
  ButtonIndex pageForward;
};

// Order matches CrossPointSettings::SIDE_BUTTON_LAYOUT.
constexpr SideLayoutMap kSideLayouts[] = {
    {HalGPIO::BTN_UP, HalGPIO::BTN_DOWN},
    {HalGPIO::BTN_DOWN, HalGPIO::BTN_UP},
};

// Minimal swipe-direction / edge-anchored-swipe classification. Kept local
// instead of pulling in freeink-sdk's FreeInkUI (not otherwise a biscuit
// dependency) — this is the entire bit of geometry biscuit needs from it.
enum class SwipeAxis { Left, Right, Up, Down, None };

SwipeAxis classifySwipe(const int sx, const int sy, const int ex, const int ey) {
  const int dx = ex - sx;
  const int dy = ey - sy;
  const int adx = dx < 0 ? -dx : dx;
  const int ady = dy < 0 ? -dy : dy;
  if (adx == 0 && ady == 0) return SwipeAxis::None;
  if (adx >= ady) return dx < 0 ? SwipeAxis::Left : SwipeAxis::Right;
  return dy < 0 ? SwipeAxis::Up : SwipeAxis::Down;
}

// How close to the left edge (as a fraction of screen width) a swipe must
// start to count as a Back gesture, mirroring the "reach in from the bezel"
// anchor freeink-sdk's UI helpers use for edge swipes.
constexpr float BACK_EDGE_SWIPE_FRACTION = 0.25f;

bool isLeftEdgeSwipe(const int sx, const int sy, const int ex, const int ey, const int screenWidth) {
  const int dx = ex - sx;
  const int dy = ey - sy;
  const int adx = dx < 0 ? -dx : dx;
  const int ady = dy < 0 ? -dy : dy;
  return sx <= static_cast<int>(screenWidth * BACK_EDGE_SWIPE_FRACTION) && dx > 0 && adx > ady;
}
}  // namespace

bool MappedInputManager::mapButton(const Button button, bool (HalGPIO::*fn)(uint8_t) const) const {
  const auto sideLayout = static_cast<CrossPointSettings::SIDE_BUTTON_LAYOUT>(SETTINGS.sideButtonLayout);
  const auto& side = kSideLayouts[sideLayout];

  switch (button) {
    case Button::Back:
      // Logical Back maps to user-configured front button.
      return (gpio.*fn)(SETTINGS.frontButtonBack);
    case Button::Confirm:
      // Logical Confirm maps to user-configured front button.
      return (gpio.*fn)(SETTINGS.frontButtonConfirm);
    case Button::Left:
      // Logical Left maps to user-configured front button.
      return (gpio.*fn)(SETTINGS.frontButtonLeft);
    case Button::Right:
      // Logical Right maps to user-configured front button.
      return (gpio.*fn)(SETTINGS.frontButtonRight);
    case Button::Up:
      // Side buttons remain fixed for Up/Down.
      return (gpio.*fn)(HalGPIO::BTN_UP);
    case Button::Down:
      // Side buttons remain fixed for Up/Down.
      return (gpio.*fn)(HalGPIO::BTN_DOWN);
    case Button::Power:
      // Power button bypasses remapping.
      return (gpio.*fn)(HalGPIO::BTN_POWER);
    case Button::PageBack:
      // Reader page navigation uses side buttons and can be swapped via settings.
      return (gpio.*fn)(side.pageBack);
    case Button::PageForward:
      // Reader page navigation uses side buttons and can be swapped via settings.
      return (gpio.*fn)(side.pageForward);
  }

  return false;
}

bool MappedInputManager::hasTouch() const { return gpio.hasTouch(); }

bool MappedInputManager::wasScreenTapped(int& x, int& y) const {
  float nx = 0.0f;
  float ny = 0.0f;
  if (!gpio.wasTouchTap(nx, ny)) return false;
  renderer.tapToLogical(nx, ny, x, y);
  return true;
}

bool MappedInputManager::wasTapInRect(const int x, const int y, const int width, const int height) const {
  int tx = 0;
  int ty = 0;
  return wasScreenTapped(tx, ty) && tx >= x && tx < x + width && ty >= y && ty < y + height;
}

MappedInputManager::RowTouch MappedInputManager::rowTouch(int& row, const int top, const int rowStep,
                                                          const int rowCount, const int xStart, const int xEnd,
                                                          const int rowHeight) const {
  if (rowStep <= 0 || rowCount <= 0 || !gpio.hasTouch()) return RowTouch::None;
  const auto hit = [&](const int x, const int y) {
    if (x < xStart || x >= xEnd || y < top) return false;
    const int r = (y - top) / rowStep;
    if (r >= rowCount) return false;
    if (rowHeight > 0 && (y - top) % rowStep >= rowHeight) return false;
    row = r;
    return true;
  };
  int x = 0;
  int y = 0;
  float nx = 0.0f;
  float ny = 0.0f;
  unsigned long heldMs = 0;
  if (gpio.isTouchTapCandidate(nx, ny, heldMs)) {
    renderer.tapToLogical(nx, ny, x, y);
    if (hit(x, y)) return RowTouch::Down;
  }
  if (wasScreenTapped(x, y) && hit(x, y)) return RowTouch::Tap;
  return RowTouch::None;
}

MappedInputManager::RowTouch MappedInputManager::colTouch(int& col, const int left, const int colStep,
                                                          const int colCount, const int yStart, const int yEnd,
                                                          const int colWidth) const {
  if (colStep <= 0 || colCount <= 0 || !gpio.hasTouch()) return RowTouch::None;
  const auto hit = [&](const int x, const int y) {
    if (y < yStart || y >= yEnd || x < left) return false;
    const int c = (x - left) / colStep;
    if (c >= colCount) return false;
    if (colWidth > 0 && (x - left) % colStep >= colWidth) return false;
    col = c;
    return true;
  };
  int x = 0;
  int y = 0;
  float nx = 0.0f;
  float ny = 0.0f;
  unsigned long heldMs = 0;
  if (gpio.isTouchTapCandidate(nx, ny, heldMs)) {
    renderer.tapToLogical(nx, ny, x, y);
    if (hit(x, y)) return RowTouch::Down;
  }
  if (wasScreenTapped(x, y) && hit(x, y)) return RowTouch::Tap;
  return RowTouch::None;
}

bool MappedInputManager::decodeSwipe(int& sx, int& sy, int& ex, int& ey) const {
  float nxs = 0.0f;
  float nys = 0.0f;
  float nxe = 0.0f;
  float nye = 0.0f;
  if (!gpio.wasSwipe(nxs, nys, nxe, nye)) return false;
  renderer.tapToLogical(nxs, nys, sx, sy);
  renderer.tapToLogical(nxe, nye, ex, ey);
  return true;
}

MappedInputManager::SwipeDir MappedInputManager::wasSwipe() const {
  int sx = 0;
  int sy = 0;
  int ex = 0;
  int ey = 0;
  if (!decodeSwipe(sx, sy, ex, ey)) return SwipeDir::None;
  switch (classifySwipe(sx, sy, ex, ey)) {
    case SwipeAxis::Left:
      return SwipeDir::Left;
    case SwipeAxis::Right:
      return SwipeDir::Right;
    case SwipeAxis::Up:
      return SwipeDir::Up;
    case SwipeAxis::Down:
      return SwipeDir::Down;
    default:
      return SwipeDir::None;
  }
}

bool MappedInputManager::wasBackGesture() const {
  int sx = 0;
  int sy = 0;
  int ex = 0;
  int ey = 0;
  if (!decodeSwipe(sx, sy, ex, ey)) return false;
  return isLeftEdgeSwipe(sx, sy, ex, ey, renderer.getScreenWidth());
}

bool MappedInputManager::wasHomeKeyTapped() const { return gpio.hasHomeKey() && gpio.wasHomeKeyTapped(); }

bool MappedInputManager::wasHomeGesture() const { return gpio.hasHomeKey() && gpio.wasHomeKeyLongPressed(); }

bool MappedInputManager::wasPressed(const Button button) const {
  // Home key: a short tap goes Back (previous page), a long hold goes to the
  // home screen (handled in the main loop via wasHomeGesture()). The SDK
  // suppresses the tap when a hold already fired the long-press, so the two
  // never both trigger. Confirm has no physical key on the X4 Pro -- it comes
  // from tapping the target directly (lists, tiles, dialogs, keyboard).
  if (button == Button::Back && (wasBackGesture() || wasHomeKeyTapped())) return true;
  return mapButton(button, &HalGPIO::wasPressed);
}

bool MappedInputManager::wasReleased(const Button button) const {
  if (button == Button::Back && (wasBackGesture() || wasHomeKeyTapped())) return true;
  return mapButton(button, &HalGPIO::wasReleased);
}

bool MappedInputManager::isPressed(const Button button) const { return mapButton(button, &HalGPIO::isPressed); }

bool MappedInputManager::wasAnyPressed() const { return gpio.wasAnyPressed(); }

bool MappedInputManager::wasAnyReleased() const { return gpio.wasAnyReleased(); }

unsigned long MappedInputManager::getHeldTime() const { return gpio.getHeldTime(); }

MappedInputManager::Labels MappedInputManager::mapLabels(const char* back, const char* confirm, const char* previous,
                                                         const char* next) const {
  // Build the label order based on the configured hardware mapping.
  auto labelForHardware = [&](uint8_t hw) -> const char* {
    // Compare against configured logical roles and return the matching label.
    if (hw == SETTINGS.frontButtonBack) {
      return back;
    }
    if (hw == SETTINGS.frontButtonConfirm) {
      return confirm;
    }
    if (hw == SETTINGS.frontButtonLeft) {
      return previous;
    }
    if (hw == SETTINGS.frontButtonRight) {
      return next;
    }
    return "";
  };

  return {labelForHardware(HalGPIO::BTN_BACK), labelForHardware(HalGPIO::BTN_CONFIRM),
          labelForHardware(HalGPIO::BTN_LEFT), labelForHardware(HalGPIO::BTN_RIGHT)};
}

int MappedInputManager::getPressedFrontButton() const {
  // Scan the raw front buttons in hardware order.
  // This bypasses remapping so the remap activity can capture physical presses.
  if (gpio.wasPressed(HalGPIO::BTN_BACK)) {
    return HalGPIO::BTN_BACK;
  }
  if (gpio.wasPressed(HalGPIO::BTN_CONFIRM)) {
    return HalGPIO::BTN_CONFIRM;
  }
  if (gpio.wasPressed(HalGPIO::BTN_LEFT)) {
    return HalGPIO::BTN_LEFT;
  }
  if (gpio.wasPressed(HalGPIO::BTN_RIGHT)) {
    return HalGPIO::BTN_RIGHT;
  }
  return -1;
}
