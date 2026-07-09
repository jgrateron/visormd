#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 256   /* se duplica al llenarse */
#define LINE_BUF_SIZE    65536 /* máximo por línea (excedente se trunca) */

TextBuffer *buffer_create(void) {
    TextBuffer *buf = malloc(sizeof(TextBuffer));
    if (!buf) return NULL;

    buf->lines = malloc(sizeof(char *) * INITIAL_CAPACITY);
    if (!buf->lines) {
        free(buf);
        return NULL;
    }
    buf->count    = 0;
    buf->capacity = INITIAL_CAPACITY;
    return buf;
}

void buffer_free(TextBuffer *buf) {
    if (!buf) return;
    for (int i = 0; i < buf->count; i++)
        free(buf->lines[i]);
    free(buf->lines);
    free(buf);
}

int buffer_load_file(TextBuffer *buf, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    char line_buf[LINE_BUF_SIZE];
    while (fgets(line_buf, sizeof(line_buf), fp)) {
        /* quitar newline y carriage return */
        size_t len = strlen(line_buf);
        while (len > 0 &&
               (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r'))
            line_buf[--len] = '\0';

        /* expandir capacidad si es necesario */
        if (buf->count >= buf->capacity) {
            buf->capacity *= 2;
            char **tmp = realloc(buf->lines, sizeof(char *) * buf->capacity);
            if (!tmp) { fclose(fp); return -1; }
            buf->lines = tmp;
        }
        buf->lines[buf->count++] = strdup(line_buf);
    }
    fclose(fp);
    return 0;
}

int buffer_load_stdin(TextBuffer *buf) {
    char line_buf[LINE_BUF_SIZE];

    while (fgets(line_buf, sizeof(line_buf), stdin)) {
        /* quitar newline y carriage return */
        size_t len = strlen(line_buf);
        while (len > 0 &&
               (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r'))
            line_buf[--len] = '\0';

        /* expandir capacidad si es necesario */
        if (buf->count >= buf->capacity) {
            buf->capacity *= 2;
            char **tmp = realloc(buf->lines, sizeof(char *) * buf->capacity);
            if (!tmp) return -1;
            buf->lines = tmp;
        }
        buf->lines[buf->count++] = strdup(line_buf);
    }
    return 0;
}
