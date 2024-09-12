#ifndef ROW_OPERATIONS_H
#define ROW_OPERATIONS_H

#include "editor.h"

int editor_row_cursor_x_to_render_x(EditorRow *row, int cx);
void editor_update_row(EditorRow *row);
void editor_insert_row(int at, char *s, size_t len);
void editor_free_row(EditorRow *row);
void editor_del_row(int at);
void editor_row_insert_char(EditorRow *row, int at, int c);
void editor_row_append_string(EditorRow *row, char *s, size_t len);
void editor_row_del_char(EditorRow *row, int at);

#endif // ROW_OPERATIONS_H
