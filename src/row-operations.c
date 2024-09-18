#include <ctype.h>
#include <stdint.h>
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

  // Updating indexes
  for (int i = at + 1; i <= E.num_rows; i++) {
    E.row[i].idx++;
  }

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].r_size = 0;
  E.row[at].r_chars = NULL;

  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
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

  // Reducing indexes
  for (int j = at; j < E.num_rows - 1; j++) {
    E.row[j].idx--;
  }
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

// Filetypes

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
    "switch",    "if",        "while",   "for",     "break",
    "continue",  "return",    "else",    "struct",  "union",
    "typedef",   "static",    "enum",    "class",   "case",
    "default:|", "int|",      "long|",   "double|", "float|",
    "char|",     "unsigned|", "signed|", "void|",   NULL};

struct EditorSyntax HLDB[] = {
    {
        .filetype = "c",
        .filematch = C_HL_extensions,
        .keywords = C_HL_keywords,
        .singleline_comment_start = "//",
        .multiline_comment_start = "/*",
        .multiline_comment_end = "*/",
        .flags = HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS,
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

// Syntax Hightlight

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editor_update_syntax(EditorRow *row) {
  row->hl = realloc(row->hl, row->r_size);
  memset(row->hl, HL_NORMAL, row->r_size);

  if (E.syntax == NULL)
    return;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->singleline_comment_start;
  int scs_len = scs ? strlen(scs) : 0;

  // multiline comment
  char *mcs = E.syntax->multiline_comment_start;
  int mcs_len = mcs ? strlen(mcs) : 0;
  char *mce = E.syntax->multiline_comment_end;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

  for (int i = 0; i < row->r_size; i++) {
    char c = row->r_chars[i];
    uint8_t prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    // HL_COMMENT
    if (scs_len && !in_string && !in_comment) {
      if (!strncmp(&row->r_chars[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->r_size - i);
        break;
      }
    }

    // HL_MLCOMMENT

    if (mcs_len && mce_len && !in_string) {
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;

        // End of the multiline comment
        if (!strncmp(&row->r_chars[i], mce, mce_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
        }

        continue;
      } else if (!strncmp(&row->r_chars[i], mcs, mcs_len)) {
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

    // HL_STRING
    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;

        // Escape Sequences
        if (c == '\\' && i + 1 < row->r_size) {
          row->hl[i + 1] = HL_STRING;
          i++;
          continue;
        }
        // Is closing string
        if (c == in_string)
          in_string = 0;

        prev_sep = 1;

        continue;
      } else if (c == '"' || c == '\'' || c == '`') {
        in_string = c;
        row->hl[i] = HL_STRING;

        continue;
      }
    }

    // HL_NUMBER
    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if (isdigit(c) && (prev_sep || prev_hl == HL_NUMBER) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        prev_sep = 0;
        continue;
      }
    }

    // keywords
    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int kw_len = strlen(keywords[j]);

        int kw2 = keywords[j][kw_len - 1] == '|';
        if (kw2) {
          kw_len--;
        }

        if (!strncmp(&row->r_chars[i], keywords[j], kw_len) &&
            is_separator(row->r_chars[i + kw_len])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, kw_len);
          i += kw_len -1;

          break;
        }
      }

      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.num_rows)
    editor_update_syntax(&E.row[row->idx + 1]);
}

const char *TEXT_RESET = "\x1b[39m";

const char *TEXT_RED = "\x1b[31m";
const char *TEXT_GREEN = "\x1b[32m";
const char *TEXT_YELLOW = "\x1b[33m";
const char *TEXT_BLUE = "\x1b[34m";
const char *TEXT_MAGENTA = "\x1b[35m";
const char *TEXT_CYAN = "\x1b[36m";
const char *TEXT_WHITE = "\x1b[37m";

const char *editor_syntax_to_color(int hl) {
  switch (hl) {
  // Literals
  case HL_NUMBER:
    return TEXT_RED;
  case HL_STRING:
    return TEXT_MAGENTA;

  case HL_MATCH:
    return TEXT_BLUE;
  case HL_MLCOMMENT:
  case HL_COMMENT:
    return TEXT_CYAN;
  case HL_KEYWORD1:
    return TEXT_GREEN;
  case HL_KEYWORD2:
    return TEXT_YELLOW;

  default:
    return TEXT_WHITE;
  }
}

void editor_select_syntax_highlight(void) {
  E.syntax = NULL;
  if (E.filename == NULL)
    return;

  char *ext = strrchr(E.filename, '.');

  for (size_t i = 0; i < HLDB_ENTRIES; i++) {
    struct EditorSyntax *s = &HLDB[i];

    for (size_t j = 0; s->filematch[i]; i++) {
      int is_ext = (s->filematch[i][0] == '.');
      int ext_match = (ext && strcmp(ext, s->filematch[i]) == 0);
      int filename_contain_pattern =
          (strstr(E.filename, s->filematch[i]) != NULL);

      if ((is_ext && ext_match) || (!is_ext && filename_contain_pattern)) {
        E.syntax = s;

        for (size_t file_row = 0; file_row < E.num_rows; file_row++) {
          editor_update_syntax(&E.row[file_row]);
        }

        return;
      }
    }
  }
}
