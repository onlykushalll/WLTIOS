#ifndef FILE_DND_H
#define FILE_DND_H

int file_dnd_mark(const char *filename, const char *source_dir);
int file_dnd_paste(const char *dest_dir);
int file_dnd_is_marked(void);
const char *file_dnd_get_marked(void);

#endif
