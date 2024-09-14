#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "append_buffer.h"
#include "editor-io.h"
#include "editor-operations.h"
#include "editor.h"
#include "file-io.h"
#include "find.h"
#include "row-operations.h"
#include "terminal.h"

extern struct EditorConfig E;

const char *TEXT_RED = "\x1b[31m";
const char *TEXT_RESET = "\x1b[39m";

// Sets and bound check the editor scroll off
void editor_scroll(void) {
  E.render_x = 0;
  if (E.cursor_y < E.num_rows) {
    E.render_x =
        editor_row_cursor_x_to_render_x(&E.row[E.cursor_y], E.cursor_x);
  }

  // Cursor Y
  if (E.cursor_y < E.row_off) {
    E.row_off = E.cursor_y;
  }

  if (E.cursor_y >= E.row_off + E.screen_rows) {
    E.row_off = E.cursor_y - E.screen_rows + 1;
  }

  // Cursor X
  if (E.render_x < E.col_off) {
    E.col_off = E.render_x;
  }

  if (E.render_x >= E.col_off + E.screen_cols) {
    E.col_off = E.render_x - E.screen_cols + 1;
  }
}

void editor_draw_welcome(struct abuf *ab) {
  char welcome[80];
  int welcome_len = snprintf(welcome, sizeof(welcome),
                             "Kilo editor -- version %s", KILO_VERSION);

  if (welcome_len > E.screen_cols) {
    welcome_len = E.screen_cols;
  }

  int padding = (E.screen_cols - welcome_len) / 2;
  if (padding) {
    ab_append(ab, "~", 1);
    padding--;
  }

  for (int i = 0; i < padding; i++) {
    ab_append(ab, " ", 1);
  }

  ab_append(ab, welcome, welcome_len);
}

void editor_draw_rows(struct abuf *ab) {
  for (size_t y = 0; y < E.screen_rows; y++) {

    int file_row = y + E.row_off;
    if (file_row < E.num_rows) {
      int len = E.row[file_row].r_size - E.col_off;
      if (len < 0) {
        len = 0;
      }

      if (len > E.screen_cols) {
        len = E.screen_cols;
      }

      char *c = &E.row[file_row].r_chars[E.col_off];

      for (int j = 0; j < len; j++) {
        if isdigit (c[j]) {
          ab_append(ab, TEXT_RED, 5);
          ab_append(ab, &c[j], 1);
          ab_append(ab, TEXT_RESET, 5);
        } else {
          ab_append(ab, &c[j], 1);
        }
      }

    } else if (E.num_rows == 0 && y == E.screen_rows / 3) {
      editor_draw_welcome(ab);

    } else {
      ab_append(ab, "~", 1);
    }

    ab_append(ab, CLEAR_LINE_RIGHT, 3);

    ab_append(ab, "\r\n", 2);
  }
}

void editor_draw_status_bar(struct abuf *ab) {
  ab_append(ab, INVERSE_FORMATTING, 4);

  char status[80], r_status[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     E.filename ? E.filename : "[No Name]", E.num_rows,
                     E.dirty ? "[+]" : "");
  if (len > E.screen_cols) {
    len = E.screen_cols;
  }

  int r_len = snprintf(r_status, sizeof(r_status), "%d/%d ", E.cursor_y + 1,
                       E.num_rows);

  ab_append(ab, status, len);

  for (; len < E.screen_cols; len++) {
    if (E.screen_cols - len == r_len) {
      ab_append(ab, r_status, r_len);
      break;
    }

    ab_append(ab, " ", 1);
  }

  ab_append(ab, RESET_FORMATTING, 3);
  ab_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(struct abuf *ab) {
  ab_append(ab, CLEAR_LINE_RIGHT, 3);

  int msg_len = strlen(E.status_msg);
  if (msg_len > E.screen_cols) {
    msg_len = E.screen_cols;
  }

  if (msg_len && time(NULL) - E.status_msg_time < 5) {
    ab_append(ab, E.status_msg, msg_len);
  }
}

void editor_refresh_screen(void) {
  editor_scroll();

  struct abuf ab = ABUF_INIT;

  ab_append(&ab, CURSOR_HIDE, 6);
  ab_append(&ab, CURSOR_HOME_CMD, 3);

  editor_draw_rows(&ab);
  editor_draw_status_bar(&ab);
  editor_draw_message_bar(&ab);

  char buf[32];
  // Format cursor position escape sequence into buf
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
           (E.cursor_y - E.row_off) + 1, //
           (E.render_x - E.col_off) + 1);
  ab_append(&ab, buf, strlen(buf));

  ab_append(&ab, CURSOR_SHOW, 6);

  write(STDOUT_FILENO, ab.buffer, ab.len);
  ab_free(&ab);
}

void editor_set_status_message(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
  va_end(ap);

  E.status_msg_time = time(NULL);
}

// input

char *editor_prompt(char *prompt, void (*callback)(char *, int)) {
  size_t buf_size = 128;
  char *buf = malloc(buf_size);

  size_t buf_len = 0;
  buf[0] = '\0';

  while (1) {
    editor_set_status_message(prompt, buf);
    editor_refresh_screen();

    int c = editor_read_key();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buf_len != 0) {
        buf[--buf_len] = '\0';
      }
      continue;
    }

    if (c == ESC_KEY) {
      editor_set_status_message("");
      if (callback) {
        callback(buf, c);
      }

      free(buf);

      return NULL;
    }

    if (c == '\r' && buf_len != 0) {
      editor_set_status_message("");
      if (callback) {
        callback(buf, c);
      }

      return buf;
    }

    if (!iscntrl(c) && c < 128) {
      if (buf_len == buf_size - 1) {
        buf_size *= 2;
        buf = realloc(buf, buf_size);
      }

      buf[buf_len++] = c;
      buf[buf_len] = '\0';
    }

    if (callback) {
      callback(buf, c);
    }
  }
}

void editor_move_cursor(int key) {
  EditorRow *row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];

  switch (key) {
  // Up
  case ARROW_UP:
    if (E.cursor_y == 0) {
      return;
    }

    E.cursor_y--;
    break;

  // Down
  case ARROW_DOWN:
    if (E.cursor_y >= E.num_rows) {
      return;
    }

    E.cursor_y++;
    break;

  // Left
  case ARROW_LEFT:
    if (E.cursor_x == 0) {
      // Move cursor up to the end of the previous row if not at the top row
      if (E.cursor_y > 0) {
        E.cursor_y--;
        E.cursor_x = E.row[E.cursor_y].size;
      }

      return;
    }

    E.cursor_x--;
    break;

  // Right
  case ARROW_RIGHT:
    if (row && E.cursor_x >= row->size) {
      // Move cursor to the beginning of the next row if at the end of the
      // current row
      if (row && E.cursor_x == row->size) {
        E.cursor_y++;
        E.cursor_x = 0;
      }
      return;
    }

    E.cursor_x++;
    break;
  }

  row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
  int row_len = row ? row->size : 0;
  if (E.cursor_x > row_len) {
    E.cursor_x = row_len;
  }
}

void editor_process_keypress(void) {
  static int quit_times = KILO_QUIT_TIMES;

  int c = editor_read_key();

  switch (c) {

  case '\r':
    editor_insert_new_line();
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY) {
      editor_move_cursor(ARROW_RIGHT);
    }

    editor_del_char();
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  // Quit
  case CTRL_KEY('q'):
    if (E.dirty && quit_times > 0) {
      editor_set_status_message("WARNING! File has unsaved changse. "
                                "Press Ctrl-Q %d more times to quit",
                                quit_times);
      quit_times--;
      return;
    }

    write(STDOUT_FILENO, CLEAR_SCREEN_CMD, 4);
    write(STDOUT_FILENO, CURSOR_HOME_CMD, 3);
    exit(EXIT_SUCCESS);
    break;

  // Save
  case CTRL_KEY('s'):
    editor_save();
    break;

  // Find
  case CTRL_KEY('f'):
    editor_find();
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      E.cursor_y = E.row_off;
    } else if (c == PAGE_DOWN) {
      E.cursor_y = E.row_off + E.screen_rows - 1;
      if (E.cursor_y > E.num_rows) {
        E.cursor_y = E.num_rows;
      }
    }

    uint16_t times = E.screen_rows;
    while (times--) {
      editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
  }

  case HOME_KEY:
    E.cursor_x = 0;
    break;

  case END_KEY:
    if (E.cursor_y < E.num_rows) {
      E.cursor_x = E.row[E.cursor_y].size;
    }
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editor_move_cursor(c);
    break;

  default:
    editor_insert_char(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES;
}
