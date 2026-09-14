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
  // apps/games batch (Minesweeper, Snake, Tetris, Sudoku, GameOfLife,
  // Chess, Maze, Voronoi, MatrixRain, Calculator, UnitConverter,
  // OtpGenerator, Countdown, EtchASketch)
  STR_MINESWEEPER, STR_MINES, STR_FLAGS, STR_YOU_WIN, STR_GAME_OVER,
  STR_REVEAL, STR_NEW_GAME, STR_DIFFICULTY, STR_EASY, STR_MEDIUM, STR_HARD,
  STR_GAME_OF_LIFE, STR_RUNNING, STR_PAUSED, STR_STEP, STR_RANDOMIZE,
  STR_CHESS, STR_CHECK, STR_CHECKMATE, STR_STALEMATE, STR_WHITE, STR_BLACK_PIECE,
  STR_SUDOKU, STR_SOLVED, STR_NO_SOLUTION,
  STR_VORONOI, STR_REGENERATE,
  STR_UNIT_CONVERTER_POINTS, STR_POINTS,
  STR_RETRY,
  STR_OUT_OF_MEMORY, STR_PEN_DOWN, STR_PEN_UP,
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
    case STR_MINESWEEPER: return "Minesweeper";
    case STR_MINES: return "Mines";
    case STR_FLAGS: return "Flags";
    case STR_YOU_WIN: return "You Win!";
    case STR_GAME_OVER: return "Game Over";
    case STR_REVEAL: return "Reveal";
    case STR_NEW_GAME: return "New Game";
    case STR_DIFFICULTY: return "Difficulty";
    case STR_EASY: return "Easy";
    case STR_MEDIUM: return "Medium";
    case STR_HARD: return "Hard";
    case STR_GAME_OF_LIFE: return "Game of Life";
    case STR_RUNNING: return "Running";
    case STR_PAUSED: return "Paused";
    case STR_STEP: return "Step";
    case STR_RANDOMIZE: return "Randomize";
    case STR_CHESS: return "Chess";
    case STR_CHECK: return "Check";
    case STR_CHECKMATE: return "Checkmate";
    case STR_STALEMATE: return "Stalemate";
    case STR_WHITE: return "White";
    case STR_BLACK_PIECE: return "Black";
    case STR_SUDOKU: return "Sudoku";
    case STR_SOLVED: return "Solved!";
    case STR_NO_SOLUTION: return "No solution";
    case STR_VORONOI: return "Voronoi";
    case STR_REGENERATE: return "Regenerate";
    case STR_POINTS: return "Points";
    case STR_RETRY: return "Retry";
    case STR_OUT_OF_MEMORY: return "Out of memory";
    case STR_PEN_DOWN: return "Pen: Down";
    case STR_PEN_UP: return "Pen: Up";
    case STR_UNIT_CONVERTER: return "Unit Converter";
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
