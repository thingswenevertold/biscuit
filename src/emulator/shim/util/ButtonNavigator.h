#pragma once
// Emulator shim: redirect the firmware's
//   #include "util/ButtonNavigator.h"
// to the REAL src/util/ButtonNavigator.h (not the no-op
// test/mocks/ButtonNavigator.h used by native unit tests). ButtonNavigator
// is pure logic (no hardware deps beyond MappedInputManager, which still
// resolves to the mock via the normal -Itest/mocks search path), and
// several already-wired activities rely on its continuous-press/held-time
// behavior (onNext/onPrevious/onPressAndContinuous) for movement — the
// no-op mock silently drops that input, which is a real fidelity gap for
// the emulator (unlike native unit tests, which don't drive interactive
// input through ButtonNavigator at all). src/util/ButtonNavigator.cpp is
// compiled in by [env:emulator]'s build_src_filter; main.cpp calls
// ButtonNavigator::setMappedInputManager() once at startup.
#include "../../../util/ButtonNavigator.h"
