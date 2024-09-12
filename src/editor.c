#include "editor.h"

const char *KILO_VERSION = "0.0.1";
const size_t KILO_TAB_STOP = 4;

const int ESC_KEY = '\x1b';

const char *CLEAR_SCREEN_CMD = "\x1b[2J";
const char *CLEAR_LINE_RIGHT = "\x1b[K";

const char *RESET_FORMATTING = "\x1b[m";
const char *INVERSE_FORMATTING = "\x1b[7m";

const char *CURSOR_HOME_CMD = "\x1b[H";
const char *CURSOR_MOVE_TO_END = "\x1b[999C\x1b[999B";
const char *CURSOR_REPORT_POSITION = "\x1b[6n";
const char *CURSOR_SHOW = "\x1b[?25h";
const char *CURSOR_HIDE = "\x1b[?25l";
