#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "editor-io.h"
#include "editor.h"
#include "file-io.h"
#include "row-operations.h"
#include "terminal.h"

extern struct EditorConfig E;

char *editor_rows_to_string(int *buf_len) {
  int total_len = 0;

  for (int i = 0; i < E.num_rows; i++) {
    total_len += E.row[i].size + 1;
  }
  *buf_len = total_len;

  char *buf = malloc(total_len);
  char *p = buf;
  for (int i = 0; i < E.num_rows; i++) {
    memcpy(p, E.row[i].chars, E.row[i].size);

    p += E.row[i].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editor_open(char *filename) {
  FILE *file_pointer = fopen(filename, "r");
  if (!file_pointer) {
    die("fopen");
  }

  editor_select_syntax_highlight();

  free(E.filename);
  E.filename = strdup(filename);

  char *line = NULL;
  size_t line_cap = 0;

  ssize_t line_len;
  while ((line_len = getline(&line, &line_cap, file_pointer)) != -1) {

    while (line_len > 0) {
      char last_char = line[line_len - 1];
      if (last_char != '\n' && last_char != '\r') {
        break;
      }

      line_len--;
    }

    editor_insert_row(E.num_rows, line, line_len);
  }

  free(line);
  fclose(file_pointer);

  E.dirty = 0;
}

void editor_save(void) {
  if (E.filename == NULL) {
    E.filename = editor_prompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editor_set_status_message("Save Aborted");
      return;
    }

    editor_select_syntax_highlight();
  }

  int len;
  char *buf = editor_rows_to_string(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd == -1)
    goto cleanup;

  if (ftruncate(fd, len) == -1)
    goto cleanup;
  if (write(fd, buf, len) != len)
    goto cleanup;

  close(fd);
  free(buf);
  editor_set_status_message("%d bytes written to disk", len);

  E.dirty = 0;

  return;

cleanup:
  if (fd != -1)
    close(fd);
  free(buf);
  editor_set_status_message("Can't save! I/O error: %s", strerror(errno));
}
