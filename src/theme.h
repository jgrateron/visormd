#ifndef THEME_H
#define THEME_H

#include <stddef.h>

/* ── índices de pares de color (compartidos con renderer) ── */
#define CP_DEFAULT      1
#define CP_HEADING_1    2
#define CP_HEADING_2    3
#define CP_HEADING_3    4
#define CP_HEADING_4    5
#define CP_HEADING_5    6
#define CP_HEADING_6    7
#define CP_BOLD         8
#define CP_ITALIC       9
#define CP_CODE        10
#define CP_CODE_BLOCK  11
#define CP_BLOCKQUOTE  12
#define CP_LIST_MARKER 13
#define CP_LINK        14
#define CP_HR          15
#define CP_STATUSBAR     16
#define CP_TABLE_BORDER  17
#define CP_TABLE_HEADER  18
#define CP_HIGHLIGHT     19
#define CP_BASH_COMMAND  20
#define CP_BASH_STRING   21
#define CP_BASH_VARIABLE 22
#define CP_BASH_KEYWORD  23

#define CP_FIRST  1
#define CP_COUNT  24   /* índices 1..23, posición 0 sin usar */

/* ── un par foreground / background ── */
typedef struct {
    short fg;
    short bg;   /* -1 = color por defecto del terminal */
} ThemeColor;

/* ── una paleta de colores completa ── */
typedef struct {
    const char  *id;                    /* clave en archivo de config */
    const char  *name;                  /* nombre legible en selector */
    ThemeColor   pairs[CP_COUNT];       /* indexado por CP_* macros  */
} Theme;

/* ── acceso a temas ── */
int          theme_get_count(void);
const Theme *theme_get_by_index(int idx);
int          theme_find_by_id(const char *id);

/* ── archivo de configuración ── */
int config_get_path(char *buf, size_t bufsz);
int config_load_theme(void);
int config_save_theme(const char *theme_id);

#endif
