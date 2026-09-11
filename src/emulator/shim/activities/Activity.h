#pragma once
// Emulator shim: redirect the firmware's path-qualified
//   #include "activities/Activity.h"
// to the native mock Activity base class in test/mocks/.
//
// This shim directory (src/emulator/shim) is placed on the include path
// BEFORE src/, so real activity .cpp/.h files pick up the mock Activity
// (header-only, no ActivityManager/hardware coupling) instead of the real one.
#include "../../../../test/mocks/Activity.h"
