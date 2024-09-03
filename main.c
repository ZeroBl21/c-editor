#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <unistd.h>

// defines

#define CLEAR_SCREEN_CMD "\x1b[2J"

#define CURSOR_HOME_CMD "\x1b[H"
#define CURSOR_MOVE_TO_END "\x1b[999C\x1b[999B"
#define CURSOR_REPORT_POSITION "\x1b[6n"

#define CTRL_KEY(k) ((k) & 0x1f)

// data

struct editorConfig {
  uint16_t screen_rows;
  uint16_t screen_cols;
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

char editor_read_key(void) {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
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

  if (buf[0] != '\x1b' || buf[1] != '[') {
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

void editor_draw_rows(struct abuf *ab) {
  for (size_t y = 0; y < E.screen_rows; y++) {
    ab_append(ab, "~", sizeof("~"));

    if (y < E.screen_rows - 1) {
      ab_append(ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen(void) {
  struct abuf ab = ABUF_INIT;

  ab_append(&ab, CLEAR_SCREEN_CMD, 4);
  ab_append(&ab, CURSOR_HOME_CMD, 3);

  editor_draw_rows(&ab);

  ab_append(&ab, CURSOR_HOME_CMD, 3);

  write(STDOUT_FILENO, ab.buffer, ab.len);
  ab_free(&ab);
}

// input

void editor_process_keypress(void) {
  char c = editor_read_key();

  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, CLEAR_SCREEN_CMD, 4);
    write(STDOUT_FILENO, CURSOR_HOME_CMD, 3);
    exit(EXIT_SUCCESS);
    break;
  }
}

// init

void init_editor(void) {
  if (get_window_size(&E.screen_rows, &E.screen_cols) == -1) {
    die("get_window_size");
  }
}

int main() {
  setlocale(LC_ALL, "");

  enable_raw_mode();
  init_editor();

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return EXIT_SUCCESS;
}
