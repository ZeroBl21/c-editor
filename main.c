
#include <fcntl.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/editor-io.h"
#include "src/editor.h"
#include "src/file-io.h"
#include "src/terminal.h"

struct EditorConfig E;

// Prototypes

void editor_set_status_message(const char *fmt, ...);
void editor_refresh_screen(void);
char *editor_prompt(char *prompt);

// init

void init_editor(void) {
  E = (struct EditorConfig){
      .cursor_x = 0,
      .cursor_y = 0,
      .render_x = 0,
      .row_off = 0,
      .col_off = 0,

      .num_rows = 0,
      .row = NULL,
      .dirty = 0,

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

  editor_set_status_message("HELP: Ctrl-S = Save | Ctrl-Q = quit");

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return EXIT_SUCCESS;
}
