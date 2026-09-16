#pragma once

#include <HalGPIO.h>

class GfxRenderer;

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };
  enum class SwipeDir { None, Left, Right, Up, Down };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  // The renderer reference is only used to translate normalized touch coordinates into
  // logical (orientation-aware) screen coordinates on touch-capable boards (see
  // wasScreenTapped/rowTouch/colTouch below); button-only boards never dereference it.
  MappedInputManager(HalGPIO& gpio, const GfxRenderer& renderer) : gpio(gpio), renderer(renderer) {}

  void update() const { gpio.update(); }
  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;
  // Returns the raw front button index that was pressed this frame (or -1 if none).
  int getPressedFrontButton() const;

  // --- Touch (X4 Pro / GT911). All of these are compiled out to inert
  // false/None-returning bodies on boards without FREEINK_CAP_TOUCH, so callers
  // never need board conditionals of their own. ---
  bool hasTouch() const;
  // Tap released inside `x/y/width/height` (logical screen coordinates).
  bool wasTapInRect(int x, int y, int width, int height) const;
  // Raw tap release, anywhere on screen. Logical coordinates.
  bool wasScreenTapped(int& x, int& y) const;

  enum class RowTouch : uint8_t { None, Down, Tap };
  // Shared hit-test for a band of `rowCount` equal-height rows starting at `top`,
  // stepping `rowStep` px per row (optionally narrower than the step via
  // `rowHeight`, and bounded horizontally by [xStart, xEnd)). Down = a
  // tap-candidate is currently resting on a row (live selection highlight);
  // Tap = a tap was released on one (activate it).
  RowTouch rowTouch(int& row, int top, int rowStep, int rowCount, int xStart = 0, int xEnd = 0x7fffffff,
                    int rowHeight = 0) const;
  // Horizontal counterpart, for side-by-side controls (confirmation buttons, grid tiles' columns).
  RowTouch colTouch(int& col, int left, int colStep, int colCount, int yStart, int yEnd, int colWidth = 0) const;

  SwipeDir wasSwipe() const;
  // Left-to-right swipe starting near the screen's left edge -- the touch
  // equivalent of the logical Back button. wasPressed/wasReleased(Button::Back)
  // already fold this in, so most call sites never need it directly.
  bool wasBackGesture() const;
  // True once when the capacitive Home key (GT911 status bit) is tapped
  // (short press+release). Folded into wasPressed/wasReleased(Button::Confirm).
  bool wasHomeKeyTapped() const;
  // True once when the Home key is held past the SDK's long-press threshold.
  // Callers that want a direct "jump to the home screen" shortcut poll this
  // (ActivityManager does, so it works from anywhere without per-activity code).
  bool wasHomeGesture() const;

 private:
  HalGPIO& gpio;
  const GfxRenderer& renderer;

  bool mapButton(Button button, bool (HalGPIO::*fn)(uint8_t) const) const;
  // Fetches the pending swipe (if any) and maps both endpoints to logical screen coords.
  bool decodeSwipe(int& sx, int& sy, int& ex, int& ey) const;
};
