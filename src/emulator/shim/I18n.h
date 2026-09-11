#pragma once
// Emulator I18n shim: overrides test/mocks/I18n.h (which returns "str" for
// every key) with readable English so the emulated UI shows real labels.
// The StrId enum MUST stay value-compatible with test/mocks/I18n.h.
#include <string>

enum StrId {
  STR_NONE_OPT, STR_BACK, STR_SELECT, STR_CONFIRM, STR_CANCEL, STR_EXIT,
  STR_DIR_UP, STR_DIR_DOWN, STR_DIR_LEFT, STR_DIR_RIGHT,
  STR_DICE_ROLLER, STR_SELECT_DICE, STR_ROLL, STR_REROLL, STR_ROLLING,
  STR_MORSE_CODE, STR_ENCODE_TEXT, STR_DECODE_MORSE, STR_REFERENCE_CHART,
  STR_UNIT_CONVERTER, STR_APPS, STR_GAMES, STR_UTILITIES,
  STR_NETWORK_TOOLS, STR_WIRELESS_TESTING,
  STR_STATE_ON, STR_STATE_OFF, STR_DONE, STR_OK_BUTTON,
  STR_MAX
};

inline const char* tr(StrId id) {
  switch (id) {
    case STR_BACK: return "Back";
    case STR_SELECT: return "Select";
    case STR_CONFIRM: return "OK";
    case STR_CANCEL: return "Cancel";
    case STR_EXIT: return "Exit";
    case STR_DIR_UP: return "Up";
    case STR_DIR_DOWN: return "Down";
    case STR_DIR_LEFT: return "Left";
    case STR_DIR_RIGHT: return "Right";
    case STR_DICE_ROLLER: return "Dice Roller";
    case STR_SELECT_DICE: return "Select dice";
    case STR_ROLL: return "Roll";
    case STR_REROLL: return "Reroll";
    case STR_ROLLING: return "Rolling...";
    case STR_APPS: return "Apps";
    case STR_GAMES: return "Games";
    case STR_UTILITIES: return "Utilities";
    case STR_NETWORK_TOOLS: return "Network Tools";
    case STR_WIRELESS_TESTING: return "Wireless Testing";
    case STR_STATE_ON: return "On";
    case STR_STATE_OFF: return "Off";
    case STR_DONE: return "Done";
    case STR_OK_BUTTON: return "OK";
    default: return "?";
  }
}

struct I18nHelper {
  const char* get(StrId id) { return tr(id); }
  void setLanguage(int) {}
  int getLanguage() { return 0; }
  std::string getLanguageName(int) { return "English"; }
};

inline I18nHelper I18N;
