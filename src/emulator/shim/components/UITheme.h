#pragma once
// ============================================================
// Emulator UITheme — a DRAWING version of the test/mocks/UITheme.h
//
// The native unit-test mock (test/mocks/UITheme.h) makes every GUI.*
// helper a no-op. For the interactive emulator we want headers, button
// hints, etc. to actually appear on screen, so this shim implements the
// handful of GUIHelper methods that real activities call, drawing through
// the BitmapRenderer (GfxRenderer under EMULATOR_BUILD).
//
// It mirrors the mock's type surface (UIMetrics / UIIcon / GUIHelper /
// UITheme) so any activity that compiles against the mock also compiles
// here. Methods not needed for visuals are left as no-ops.
// ============================================================

#include "GfxRenderer.h"  // -> BitmapRenderer under EMULATOR_BUILD
#include "fontIds.h"

#include <string>
#include <functional>
#include <vector>

enum class UIIcon { Folder, Recent, Transfer, Book, Settings, Wifi, Hotspot, Library };

struct TabInfo { const char* label; bool selected; };

struct UIMetrics {
  int topPadding = 5;
  int headerHeight = 40;
  int tabBarHeight = 30;
  int verticalSpacing = 8;
  int buttonHintsHeight = 30;
  int contentSidePadding = 15;
  int progressBarHeight = 8;
  int statusBarVerticalMargin = 4;
  int listRowHeight = 35;
  int homeTopPadding = 50;
  int homeCoverTileHeight = 200;
  int homeCoverHeight = 180;
  int homeRecentBooksCount = 4;
  bool keyboardCenteredText = false;
  bool keyboardBottomAligned = true;
  int keyboardKeyWidth = 32;
  int keyboardKeyHeight = 28;
  int keyboardKeySpacing = 3;
};

struct GUIHelper {
  // --- implemented for visuals ---
  void drawHeader(GfxRenderer& r, Rect area, const char* title, const char* subtitle = nullptr) {
    int titleY = area.y + 7;
    r.drawCenteredText(UI_12_FONT_ID, titleY, title, true, /*BOLD*/ 1);
    int lineY = area.y + area.h - 2;
    r.drawLine(area.x + 15, lineY, area.x + area.w - 15, lineY);
    if (subtitle) r.drawCenteredText(SMALL_FONT_ID, lineY + 6, subtitle);
  }

  void drawButtonHints(GfxRenderer& r, const char* b1, const char* b2, const char* b3, const char* b4) {
    const int sw = r.getScreenWidth();
    const int sh = r.getScreenHeight();
    const int y = sh - 25;
    r.drawLine(0, y - 7, sw, y - 7);
    if (b1 && *b1) r.drawText(SMALL_FONT_ID, 15, y, b1);
    if (b2 && *b2) r.drawCenteredText(SMALL_FONT_ID, y, b2);
    // b3 sits just left of b4 on the right side
    if (b4 && *b4) {
      int w = r.getTextWidth(SMALL_FONT_ID, b4);
      r.drawText(SMALL_FONT_ID, sw - 15 - w, y, b4);
      if (b3 && *b3) {
        int w3 = r.getTextWidth(SMALL_FONT_ID, b3);
        r.drawText(SMALL_FONT_ID, sw - 15 - w - 20 - w3, y, b3);
      }
    } else if (b3 && *b3) {
      int w3 = r.getTextWidth(SMALL_FONT_ID, b3);
      r.drawText(SMALL_FONT_ID, sw - 15 - w3, y, b3);
    }
  }

  void drawSideButtonHints(GfxRenderer& r, const char* top, const char* bottom) {
    const int sw = r.getScreenWidth();
    if (top && *top) {
      int w = r.getTextWidth(SMALL_FONT_ID, top);
      r.drawText(SMALL_FONT_ID, sw - 5 - w, 120, top);
    }
    if (bottom && *bottom) {
      int w = r.getTextWidth(SMALL_FONT_ID, bottom);
      r.drawText(SMALL_FONT_ID, sw - 5 - w, 680, bottom);
    }
  }

  // --- no-ops (type surface parity with the mock) ---
  void drawSubHeader(GfxRenderer&, Rect, const char*, const char* = nullptr) {}
  Rect drawPopup(GfxRenderer&, const char*) { return {}; }
  void fillPopupProgress(GfxRenderer&, Rect, int) {}
  void drawProgressBar(GfxRenderer&, Rect, int, int) {}
  void drawStatusBar(GfxRenderer&, float, int, int, const std::string&, int = 0, int = 0) {}
  void drawHelpText(GfxRenderer&, Rect, const char*) {}
  void drawTextField(GfxRenderer&, Rect, int) {}
  void drawKeyboardKey(GfxRenderer&, Rect, const char*, bool) {}
  void drawList(GfxRenderer&, Rect, int, int,
      std::function<std::string(int)>,
      std::function<std::string(int)> = nullptr,
      std::function<UIIcon(int)> = nullptr,
      std::function<std::string(int)> = nullptr,
      bool = false) {}
  void drawTabBar(GfxRenderer&, Rect, const std::vector<TabInfo>&, bool = false) {}
  void drawRecentBookCover(GfxRenderer&, Rect, auto&, int, bool&, bool&, bool, auto) {}
  void drawButtonMenu(GfxRenderer&, Rect, int, int, std::function<std::string(int)>, std::function<UIIcon(int)>) {}
};

class UITheme {
 public:
  static UITheme& getInstance() { static UITheme t; return t; }
  const UIMetrics& getMetrics() const { return metrics; }
  const auto& getTheme() const { return *this; }
  bool showsFileIcons() const { return false; }
  void reload() {}
  uint8_t getStatusBarHeight() const { return 20; }
  uint8_t getProgressBarHeight() const { return 4; }
  int getNumberOfItemsPerPage(GfxRenderer&, bool = false, bool = false, bool = false, bool = false) { return 10; }
  static UIIcon getFileIcon(const std::string&) { return UIIcon::Book; }
  static std::string getCoverThumbPath(const std::string& p, int) { return p; }
 private:
  UIMetrics metrics;
};

// Global GUI singleton
inline GUIHelper GUI;
