#ifndef BUFFER_H
#define BUFFER_H

/* TextBuffer: almacena las líneas crudas leídas de un archivo */
typedef struct {
    char **lines;      /* arreglo dinámico de strings */
    int    count;      /* cantidad de líneas */
    int    capacity;   /* capacidad actual del arreglo */
} TextBuffer;

TextBuffer *buffer_create(void);
void        buffer_free(TextBuffer *buf);
int         buffer_load_file(TextBuffer *buf, const char *filename);
int         buffer_load_stdin(TextBuffer *buf);

#endif
