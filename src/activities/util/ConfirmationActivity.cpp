#include "ConfirmationActivity.h"

#include <I18n.h>

#include "../../components/UITheme.h"
#include "HalDisplay.h"

ConfirmationActivity::ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string& heading, const std::string& body)
    : Activity("Confirmation", renderer, mappedInput), heading(heading), body(body) {}

void ConfirmationActivity::onEnter() {
  Activity::onEnter();

  lineHeight = renderer.getLineHeight(fontId);
  const int maxWidth = renderer.getScreenWidth() - (margin * 2);

  if (!heading.empty()) {
    safeHeading = renderer.truncatedText(fontId, heading.c_str(), maxWidth, EpdFontFamily::BOLD);
  }
  if (!body.empty()) {
    safeBody = renderer.truncatedText(fontId, body.c_str(), maxWidth, EpdFontFamily::REGULAR);
  }

  int totalHeight = 0;
  if (!safeHeading.empty()) totalHeight += lineHeight;
  if (!safeBody.empty()) totalHeight += lineHeight;
  if (!safeHeading.empty() && !safeBody.empty()) totalHeight += spacing;

  startY = (renderer.getScreenHeight() - totalHeight) / 2;

  requestUpdate(true);
}

void ConfirmationActivity::render(RenderLock&& lock) {
  renderer.clearScreen();

  int currentY = startY;
  LOG_DBG("CONF", "currentY: %d", currentY);
  // Draw Heading
  if (!safeHeading.empty()) {
    renderer.drawCenteredText(fontId, currentY, safeHeading.c_str(), true, EpdFontFamily::BOLD);
    currentY += lineHeight + spacing;
  }

  // Draw Body
  if (!safeBody.empty()) {
    renderer.drawCenteredText(fontId, currentY, safeBody.c_str(), true, EpdFontFamily::REGULAR);
  }

  // Draw UI Elements. Touch boards hide the physical hint bar (see
  // BaseTheme::drawButtonHints), so draw explicit Cancel/Confirm tap targets in
  // its place instead -- otherwise there would be nothing on screen to tap.
  if (mappedInput.hasTouch()) {
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();
    const int barY = pageHeight - touchBarHeight;
    const int half = pageWidth / 2;
    const char* cancelLabel = I18N.get(StrId::STR_CANCEL);
    const char* confirmLabel = I18N.get(StrId::STR_CONFIRM);

    renderer.drawRect(0, barY, half, touchBarHeight);
    renderer.drawRect(half, barY, pageWidth - half, touchBarHeight);
    const int textY = barY + (touchBarHeight - lineHeight) / 2;
    const int cancelW = renderer.getTextWidth(fontId, cancelLabel);
    renderer.drawText(fontId, (half - cancelW) / 2, textY, cancelLabel);
    const int confirmW = renderer.getTextWidth(fontId, confirmLabel);
    renderer.drawText(fontId, half + (pageWidth - half - confirmW) / 2, textY, confirmLabel);
  } else {
    const auto labels = mappedInput.mapLabels("", "", I18N.get(StrId::STR_CANCEL), I18N.get(StrId::STR_CONFIRM));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void ConfirmationActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    ActivityResult res;
    res.isCancelled = false;
    setResult(std::move(res));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    ActivityResult res;
    res.isCancelled = true;
    setResult(std::move(res));
    finish();
    return;
  }

  // Touch: tap the on-screen Cancel/Confirm target drawn in render() above.
  if (mappedInput.hasTouch()) {
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();
    const int barY = pageHeight - touchBarHeight;
    int col = 0;
    if (mappedInput.colTouch(col, 0, pageWidth / 2, 2, barY, pageHeight) == MappedInputManager::RowTouch::Tap) {
      ActivityResult res;
      res.isCancelled = (col == 0);
      setResult(std::move(res));
      finish();
      return;
    }
  }
}