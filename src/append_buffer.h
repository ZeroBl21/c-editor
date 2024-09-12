#ifndef APPEND_BUFFER_H
#define APPEND_BUFFER_H

#define ABUF_INIT                                                              \
  { NULL, 0 }

struct abuf {
  char *buffer;
  int len;
};

void ab_append(struct abuf *ab, const char *s, int len);
void ab_free(struct abuf *ab);

#endif // APPEND_BUFFER_H
