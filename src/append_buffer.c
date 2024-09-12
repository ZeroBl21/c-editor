// append buffer

#include <stdlib.h>
#include <string.h>

#include "append_buffer.h"

void ab_append(struct abuf *ab, const char *s, int len) {
  char *new_buffer = realloc(ab->buffer, ab->len + len);

  if (new_buffer == NULL) {
    return;
  }

  memcpy(&new_buffer[ab->len], s, len);

  ab->buffer = new_buffer;
  ab->len += len;
}

void ab_free(struct abuf *ab) {
  free(ab->buffer);
  ab->buffer = NULL;
  ab->len = 0;
}
