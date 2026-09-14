#pragma once
#include <cstdint>
#include <string>

struct ButtonLabels {
  const char* btn1; const char* btn2; const char* btn3; const char* btn4;
};

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Up, Down, Left, Right, PageForward, PageBack, Power };

  // Simulated state for tests
  Button lastPressed = Button::Back;
  mutable bool pressedFlag = false;
  mutable bool releasedFlag = false;
  unsigned long heldTime = 0;

  // const: the real (hardware) MappedInputManager's wasPressed/wasReleased/
  // isPressed/getHeldTime are const-qualified (ButtonNavigator holds a
  // `const MappedInputManager*`), so this mock must match that signature
  // even though it mutates edge-triggered flags — hence `mutable` above.
  bool wasPressed(Button b) const { if (b == lastPressed && pressedFlag) { pressedFlag = false; return true; } return false; }
  bool wasReleased(Button b) const { if (b == lastPressed && releasedFlag) { releasedFlag = false; return true; } return false; }
  bool isPressed(Button) const { return false; }
  unsigned long getHeldTime() const { return heldTime; }
  int getPressedFrontButton() const { return -1; }
  void update() {}

  ButtonLabels mapLabels(const char* a, const char* b, const char* c, const char* d) {
    return {a, b, c, d};
  }

  // Test helpers
  void simulatePress(Button b) { lastPressed = b; pressedFlag = true; releasedFlag = false; }
  void simulateRelease(Button b) { lastPressed = b; releasedFlag = true; pressedFlag = false; }
};
