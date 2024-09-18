#ifndef ROW_OPERATIONS_H
#define ROW_OPERATIONS_H

#include "editor.h"

int editor_row_cursor_x_to_render_x(EditorRow *row, int cx);
int editor_row_render_x_to_cursor_x(EditorRow *row, int rx);
void editor_update_row(EditorRow *row);
void editor_insert_row(int at, char *s, size_t len);
void editor_free_row(EditorRow *row);
void editor_del_row(int at);
void editor_row_insert_char(EditorRow *row, int at, int c);
void editor_row_append_string(EditorRow *row, char *s, size_t len);
void editor_row_del_char(EditorRow *row, int at);

// Syntax Hightlight

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

struct EditorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;

  int flags;
};

extern const char *TEXT_RESET;

extern const char *TEXT_RED;
extern const char *TEXT_WHITE;
extern const char *TEXT_GREEN;
extern const char *TEXT_YELLOW;
extern const char *TEXT_BLUE;
extern const char *TEXT_MAGENTA;
extern const char *TEXT_CYAN;

void editor_update_syntax(EditorRow *row);
const char *editor_syntax_to_color(int hl);
void editor_select_syntax_highlight(void);

#endif // ROW_OPERATIONS_H
