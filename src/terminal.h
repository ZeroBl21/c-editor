#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <termios.h>

void die(const char *s);
void disable_raw_mode(void);
void enable_raw_mode(void);
int read_escape_sequence(char *seq, int length);
int handle_bracket_sequences(char seq[]);
int handle_o_sequences(char seq[]);
int editor_read_key(void);
int get_cursor_position(uint16_t *rows, uint16_t *cols);
int get_window_size(uint16_t *rows, uint16_t *cols);

#endif // TERMINAL_H
