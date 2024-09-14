#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "row-operations.h"

extern struct EditorConfig E;

int editor_row_cursor_x_to_render_x(EditorRow *row, int cx) {
  int rx = 0;

  for (int i = 0; i < cx; i++) {
    if (row->chars[i] == '\t') {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }

    rx++;
  }

  return rx;
}

int editor_row_render_x_to_cursor_x(EditorRow *row, int rx) {
  int cur_rx = 0;

  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t') {
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
    }

    cur_rx++;

    if (cur_rx > rx)
      return cx;
  }

  return cx;
}

void editor_update_row(EditorRow *row) {
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

  editor_update_syntax(row);
}

void editor_insert_row(int at, char *s, size_t len) {
  if (at < 0 || at > E.num_rows) {
    return;
  }

  E.row = realloc(E.row, sizeof(EditorRow) * (E.num_rows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(EditorRow) * (E.num_rows - at));

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);

  memcpy(E.row[at].chars, s, len);

  E.row[at].chars[len] = '\0';

  E.row[at].r_size = 0;
  E.row[at].r_chars = NULL;
  E.row[at].hl = NULL;
  editor_update_row(&E.row[at]);

  E.num_rows++;
  E.dirty++;
}

void editor_free_row(EditorRow *row) {
  free(row->r_chars);
  free(row->chars);
  free(row->hl);
}

void editor_del_row(int at) {
  if (at < 0 || at >= E.num_rows) {
    return;
  }

  editor_free_row(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1],
          sizeof(EditorRow) * (E.num_rows - at - 1));

  E.num_rows--;
  E.dirty++;
}

void editor_row_insert_char(EditorRow *row, int at, int c) {
  if (at < 0 || at > row->size) {
    at = row->size;
  }

  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;

  row->chars[at] = c;
  editor_update_row(row);

  E.dirty++;
}

void editor_row_append_string(EditorRow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);

  row->size += len;
  row->chars[row->size] = '\0';
  editor_update_row(row);

  E.dirty++;
}

void editor_row_del_char(EditorRow *row, int at) {
  if (at < 0 || at >= row->size) {
    return;
  }

  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editor_update_row(row);

  E.dirty++;
}

// Syntax Hightlight

void editor_update_syntax(EditorRow *row) {
  row->hl = realloc(row->hl, row->r_size);
  memset(row->hl, HL_NORMAL, row->r_size);

  for (int i = 0; i < row->r_size; i++) {
    if (isdigit(row->r_chars[i])) {
      row->hl[i] = HL_NUMBER;
    }
  }
}

const char *TEXT_RESET = "\x1b[39m";

const char *TEXT_RED = "\x1b[31m";
const char *TEXT_BLUE = "\x1b[34m";
const char *TEXT_WHITE = "\x1b[37m";

const char *editor_syntax_to_color(int hl) {
  switch (hl) {
  case HL_NUMBER:
    return TEXT_RED;
  case HL_MATCH:
    return TEXT_BLUE;
  default:
    return TEXT_WHITE;
  }
}
