#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>

// Defines

#define KILO_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)

// Constants

extern const char *KILO_VERSION;
extern const size_t KILO_TAB_STOP;

extern const int ESC_KEY;

extern const char *CLEAR_SCREEN_CMD;
extern const char *CLEAR_LINE_RIGHT;

extern const char *RESET_FORMATTING;
extern const char *INVERSE_FORMATTING;

extern const char *CURSOR_HOME_CMD;
extern const char *CURSOR_MOVE_TO_END;
extern const char *CURSOR_REPORT_POSITION;
extern const char *CURSOR_SHOW;
extern const char *CURSOR_HIDE;

enum editorKey {
  BACKSPACE = 127,

  ARROW_UP = 1000,
  ARROW_DOWN,
  ARROW_LEFT,
  ARROW_RIGHT,

  HOME_KEY,
  END_KEY,
  DEL_KEY,

  PAGE_UP,
  PAGE_DOWN,
};

enum editorHighlight { HL_NORMAL = 0, HL_NUMBER, HL_STRING, HL_MATCH };

// Types

typedef struct {
  int size;
  int r_size;
  char *chars;
  char *r_chars;

  uint8_t *hl;
} EditorRow;

struct EditorConfig {
  // Position
  int cursor_x;
  int cursor_y;
  int render_x;
  int row_off;
  int col_off;

  uint16_t screen_rows;
  uint16_t screen_cols;

  int num_rows;
  EditorRow *row;

  // File
  char *filename;
  int dirty;

  // Status Bar
  char status_msg[80];
  time_t status_msg_time;

  // Metadata
  struct EditorSyntax *syntax;
  struct termios orig_termios;
};

#endif
