#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <wctype.h>

#include <unistd.h>
#include <wchar.h>

// data

struct termios orig_termios;

// terminal

void die(const char *s) {
  perror(s);
  exit(EXIT_FAILURE);
}

void disable_raw_mode(void) {
  // Discards any unread input before applying the changes to the terminal.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    die("tcsetattr");
  }

  atexit(disable_raw_mode);

  struct termios raw = orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_oflag |= ~(CS8);
  // Disable terminal echoing, read input byte-by-byte and ignore signals.
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

// init

int main() {
  setlocale(LC_ALL, "");

  enable_raw_mode();

  while (1) {
    wchar_t c = L'\0';
    c = fgetwc(stdin);
    if (c == (wchar_t)WEOF && errno != EAGAIN) {
      if (feof(stdin)) {
        die("fgetwc - EOF");
      } else if (ferror(stdin)) {
        die("fgetwc - error reading");
      }
    }

    if (iswcntrl(c)) {
      wprintf(L"%d\r\n", c);
    } else {
      wprintf(L"%d ('%lc')\r\n", c, c);
    }

    if (c == L'q')
      break;
  }

  return EXIT_SUCCESS;
}
