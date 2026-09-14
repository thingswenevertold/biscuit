#pragma once
// Standalone mock so real source that does `#include <EpdFontFamily.h>` or
// `#include "EpdFontFamily.h"` (angle/quote, resolved via -I search path)
// compiles natively. BitmapRenderer.h includes this too, so both paths
// land on the exact same definition (no ODR risk).
namespace EpdFontFamily {
  enum Style { REGULAR = 0, BOLD = 1, ITALIC = 2 };
}
