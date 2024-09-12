#ifndef FILE_IO_H
#define FILE_IO_H

char *editor_rows_to_string(int *buf_len);
void editor_open(char *filename);
void editor_save(void);

#endif // FILE_IO_H
