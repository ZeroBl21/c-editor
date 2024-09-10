#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>

#include <unistd.h>

// defines

#define CTRL_KEY(k) ((k) & 0x1f)

// Constants

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

// types

typedef struct editorRow {
  int size;
  int r_size;
  char *chars;
  char *r_chars;
} editorRow;

struct editorConfig {
  // Position
  int cursor_x;
  int cursor_y;
  int render_x;
  int row_off;
  int col_off;

  uint16_t screen_rows;
  uint16_t screen_cols;

  int num_rows;
  editorRow *row;

  // Metadata
  char *filename;
  char status_msg[80];
  time_t status_msg_time;
  struct termios orig_termios;
} E;

// terminal

void die(const char *s) {
  write(STDOUT_FILENO, CLEAR_SCREEN_CMD, 4);
  write(STDOUT_FILENO, CURSOR_HOME_CMD, 3);

  perror(s);
  exit(EXIT_FAILURE);
}

void disable_raw_mode(void) {
  // Discards any unread input before applying the changes to the terminal.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcsetattr");
  }

  atexit(disable_raw_mode);

  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_oflag |= (CS8);
  // Disable terminal echoing, read input byte-by-byte and ignore signals.
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 10;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

int read_escape_sequence(char *seq, int length) {
  for (int i = 0; i < length; i++) {
    if (read(STDIN_FILENO, &seq[i], 1) != 1) {
      return 0; // Si la lectura falla, retorna falso
    }
  }
  return 1;
}

int handle_bracket_sequences(char seq[]) {
  // numeric
  if (seq[1] >= '0' && seq[1] <= '9') {
    if (!read_escape_sequence(&seq[2], 1) || seq[2] != '~') {
      return ESC_KEY;
    }

    switch (seq[1]) {
    case '1':
    case '7':
      return HOME_KEY;

    case '4':
    case '8':
      return END_KEY;

    case '3':
      return DEL_KEY;
    case '5':
      return PAGE_UP;
    case '6':
      return PAGE_DOWN;
    default:
      return ESC_KEY;
    }
  }

  // non-numeric
  switch (seq[1]) {
  case 'A':
    return ARROW_UP;
  case 'B':
    return ARROW_DOWN;
  case 'C':
    return ARROW_RIGHT;
  case 'D':
    return ARROW_LEFT;

  case 'H':
    return HOME_KEY;
  case 'F':
    return END_KEY;
  default:
    return ESC_KEY;
  }
}

int handle_o_sequences(char seq[]) {
  switch (seq[1]) {
  case 'H':
    return HOME_KEY;
  case 'F':
    return END_KEY;
  default:
    return ESC_KEY;
  }
}

int editor_read_key(void) {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  if (c == ESC_KEY) {
    char seq[3];

    if (!read_escape_sequence(seq, 2)) {
      return ESC_KEY;
    }

    if (seq[0] == '[') {
      return handle_bracket_sequences(seq);
    }
    if (seq[0] == 'O') {
      return handle_o_sequences(seq);
    }
  }

  return c;
}

int get_cursor_position(uint16_t *rows, uint16_t *cols) {
  char buf[32];
  uint32_t i = 0;

  if (write(STDOUT_FILENO, CURSOR_REPORT_POSITION, 4) != 4) {
    return -1;
  }

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if (buf[i] == 'R') {
      break;
    }

    i++;
  }
  buf[i] = L'\0';

  if (buf[0] != ESC_KEY || buf[1] != '[') {
    return -1;
  }
  if (sscanf(&buf[2], "%d;%d", (int *)rows, (int *)cols) != 2) {
    return -1;
  }

  return 0;
}

int get_window_size(uint16_t *rows, uint16_t *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // Fallback to get window size moving the cursor to the end of the window.

    if (write(STDOUT_FILENO, CURSOR_MOVE_TO_END, 12) != 12) {
      return -1;
    }

    return get_cursor_position(rows, cols);
  }

  *cols = ws.ws_col;
  *rows = ws.ws_row;

  return 0;
}

// row operations

int editor_row_cursor_x_to_render_x(editorRow *row, int cx) {
  int rx = 0;

  for (int i = 0; i < cx; i++) {
    if (row->chars[i] == '\t') {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }

    rx++;
  }

  return rx;
}

void editor_update_row(editorRow *row) {
  int tabs = 0;

  for (int i = 0; i < row->size; i++) {
    if (row->chars[i] == '\t') {
      tabs++;
    }
  }

  free(row->r_chars);
  row->r_chars = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

  size_t idx = 0;
  for (size_t j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->r_chars[idx++] = ' ';

      while (idx % KILO_TAB_STOP != 0) {
        row->r_chars[idx++] = ' ';
      }

      continue;
    }

    row->r_chars[idx++] = row->chars[j];
  }

  row->r_chars[idx] = '\0';
  row->r_size = idx;
}

void editor_append_row(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(editorRow) * (E.num_rows + 1));

  int at = E.num_rows;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);

  memcpy(E.row[at].chars, s, len);

  E.row[at].chars[len] = '\0';

  E.row[at].r_size = 0;
  E.row[at].r_chars = NULL;
  editor_update_row(&E.row[at]);

  E.num_rows++;
}

void editor_row_insert_char(editorRow *row, int at, int c) {
  if (at < 0 || at > row->size) {
    at = row->size;
  }

  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;

  row->chars[at] = c;
  editor_update_row(row);
}

// editor operations

void editor_insert_char(int c) {
  if (E.cursor_y == E.num_rows) {
    editor_append_row("", 0);
  }
  editor_row_insert_char(&E.row[E.cursor_y], E.cursor_x, c);

  E.cursor_x++;
}

// file i/o

void editor_open(char *filename) {
  FILE *file_pointer = fopen(filename, "r");
  if (!file_pointer) {
    die("fopen");
  }

  free(E.filename);
  E.filename = strdup(filename);

  char *line = NULL;
  size_t line_cap = 0;

  ssize_t line_len;
  while ((line_len = getline(&line, &line_cap, file_pointer)) != -1) {

    while (line_len > 0) {
      char last_char = line[line_len - 1];
      if (last_char != '\n' && last_char != '\r') {
        break;
      }

      line_len--;
    }

    editor_append_row(line, line_len);
  }

  free(line);
  fclose(file_pointer);
}

// append buffer

#define ABUF_INIT                                                              \
  { NULL, 0 }

struct abuf {
  char *buffer;
  int len;
};

void ab_append(struct abuf *ab, const char *s, int len) {
  char *new_buffer = realloc(ab->buffer, ab->len + len);

  if (new_buffer == NULL) {
    return;
  }

  memcpy(&new_buffer[ab->len], s, len);

  ab->buffer = new_buffer;
  ab->len += len;
}

void ab_free(struct abuf *ab) {
  free(ab->buffer);
  ab->buffer = NULL;
  ab->len = 0;
}

// output

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
      ab_append(ab, &E.row[file_row].r_chars[E.col_off], len);

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
  int len = snprintf(status, sizeof(status), "%.20s - %d lines",
                     E.filename ? E.filename : "[No Name]", E.num_rows);
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

void editor_move_cursor(int key) {
  editorRow *row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];

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
    if (E.cursor_y > E.num_rows) {
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
  int c = editor_read_key();

  switch (c) {

  case '\r':
    // TODO
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    // TODO
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  case CTRL_KEY('q'):
    write(STDOUT_FILENO, CLEAR_SCREEN_CMD, 4);
    write(STDOUT_FILENO, CURSOR_HOME_CMD, 3);
    exit(EXIT_SUCCESS);
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
}

// init

void init_editor(void) {
  E = (struct editorConfig){
      .cursor_x = 0,
      .cursor_y = 0,
      .render_x = 0,
      .row_off = 0,
      .col_off = 0,
      .num_rows = 0,
      .row = NULL,

      .filename = NULL,
      .status_msg = {'\0'},
      .status_msg_time = 0,
  };

  if (get_window_size(&E.screen_rows, &E.screen_cols) == -1) {
    die("get_window_size");
  }
  E.screen_rows -= 2;
}

int main(int argc, char *argv[]) {
  setlocale(LC_ALL, "");

  enable_raw_mode();
  init_editor();

  if (argc >= 2) {
    editor_open(argv[1]);
  }

  editor_set_status_message("HELP: Ctrl-Q = quit");

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return EXIT_SUCCESS;
}
