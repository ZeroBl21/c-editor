#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "src/editor-io.h"
#include "src/editor.h"
#include "src/file-io.h"
#include "src/row-operations.h"
#include "src/terminal.h"

struct EditorConfig E;

// Find
void editor_find_callback(char *query, int key) {
  if (key == '\r' || key == ESC_KEY) {
    return;
  }

  for (int i = 0; i < E.num_rows; i++) {
    EditorRow *row = &E.row[i];

    char *match = strstr(row->r_chars, query);
    if (match) {
      E.cursor_y = i;
      E.cursor_x = editor_row_render_x_to_cursor_x(row, match - row->r_chars);
      E.row_off = E.num_rows;
      break;
    }
  }
}

void editor_find(void) {
  int saved_cx = E.cursor_x;
  int saved_cy = E.cursor_y;
  int saved_col_off = E.col_off;
  int saved_row_off = E.row_off;

  char *query =
      editor_prompt("Search: %s (ESC to cancel)", editor_find_callback);
  if (query == NULL) {
    E.cursor_x = saved_cx;
    E.cursor_y = saved_cy;
    E.col_off = saved_col_off;
    E.row_off = saved_row_off;

    return;
  }

  free(query);
}

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

  editor_set_status_message(
      "HELP: Ctrl-S = Save | Ctrl-Q = Quit | Ctrl+F = Find");

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return EXIT_SUCCESS;
}
