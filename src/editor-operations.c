#include "editor.h"
#include "row-operations.h"

extern struct EditorConfig E;

void editor_insert_char(int c) {
  if (E.cursor_y == E.num_rows) {
    editor_insert_row(E.num_rows, "", 0);
  }
  editor_row_insert_char(&E.row[E.cursor_y], E.cursor_x, c);

  E.cursor_x++;
}

void editor_insert_new_line(void) {
  if (E.cursor_x == 0) {
    editor_insert_row(E.cursor_y, "", 0);
    E.cursor_y++;
    E.cursor_x = 0;

    return;
  }

  EditorRow *row = &E.row[E.cursor_y];
  editor_insert_row(E.cursor_y + 1, &row->chars[E.cursor_x],
                    row->size - E.cursor_x);

  row = &E.row[E.cursor_y];
  row->size = E.cursor_x;
  row->chars[row->size] = '\0';
  editor_update_row(row);

  E.cursor_y++;
  E.cursor_x = 0;
}

void editor_del_char(void) {
  if (E.cursor_y == E.num_rows) {
    return;
  }
  if (E.cursor_x == 0 && E.cursor_y == 0) {
    return;
  }

  EditorRow *row = &E.row[E.cursor_y];
  if (E.cursor_x > 0) {
    editor_row_del_char(row, E.cursor_x - 1);
    E.cursor_x--;

    return;
  }

  E.cursor_x = E.row[E.cursor_y - 1].size;
  editor_row_append_string(&E.row[E.cursor_y - 1], row->chars, row->size);
  editor_del_row(E.cursor_y);

  E.cursor_y--;
}
