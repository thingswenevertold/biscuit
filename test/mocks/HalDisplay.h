#pragma once
// Standalone mock so real source that does `#include <HalDisplay.h>` or
// `#include "HalDisplay.h"` (angle/quote, resolved via -I search path)
// compiles natively. BitmapRenderer.h includes this too, so both paths
// land on the exact same definition (no ODR risk).
//
// The real lib/hal/HalDisplay.h transitively pulls in Arduino.h, which is
// where LOG_DBG/LOG_ERR end up ambiently available for code (like
// ConfirmationActivity.cpp) that uses them without including Logging.h
// directly. Mirror that here so the same call sites compile.
#include "Logging.h"

namespace HalDisplay {
  enum RefreshMode { FAST_REFRESH = 0, HALF_REFRESH = 1, FULL_REFRESH = 2 };
}
