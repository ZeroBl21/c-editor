#ifndef EDITOR_IO_H
#define EDITOR_IO_H

#include "append_buffer.h"

// output

void editor_scroll(void);
void editor_draw_welcome(struct abuf *ab);
void editor_draw_rows(struct abuf *ab);
void editor_draw_status_bar(struct abuf *ab);
void editor_draw_message_bar(struct abuf *ab);
void editor_refresh_screen(void);
void editor_set_status_message(const char *fmt, ...);

// input

void editor_set_status_message(const char *fmt, ...);
void editor_refresh_screen(void);
char *editor_prompt(char *prompt, void (*callback)(char *, int));
void editor_process_keypress(void);

#endif // EDITOR_IO_H
