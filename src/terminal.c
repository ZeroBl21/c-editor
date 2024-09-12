#include "terminal.h"
#include "editor.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

extern struct EditorConfig E;

void die(const char *s) {
  write(STDOUT_FILENO, CLEAR_SCREEN_CMD, 4);
  write(STDOUT_FILENO, CURSOR_HOME_CMD, 3);

  perror(s);
  exit(EXIT_FAILURE);
}

void disable_raw_mode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }

  atexit(disable_raw_mode);

  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
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
      return 0;
    }
  }
  return 1;
}

int handle_bracket_sequences(char seq[]) {
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
  buf[i] = '\0';

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
    if (write(STDOUT_FILENO, CURSOR_MOVE_TO_END, 12) != 12) {
      return -1;
    }
    return get_cursor_position(rows, cols);
  }

  *cols = ws.ws_col;
  *rows = ws.ws_row;

  return 0;
}
