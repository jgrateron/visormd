#include "theme.h"
#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ══════════════════════════════════════════════════════════════
 * paletas de colores (9 temas)
 *
 * Cada tema define 16 pares (fg, bg).  bg = -1 significa
 * "usar color de fondo por defecto del terminal".
 * ══════════════════════════════════════════════════════════════ */

static const Theme themes[] = {

    /* ── 0: Default (colores originales de visorMD) ── */
    {
        .id   = "default",
        .name = "Default",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_6]    = { COLOR_MAGENTA, -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_GREEN,   -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLUE },
            [CP_BLOCKQUOTE]   = { COLOR_CYAN,    -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_WHITE },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_WHITE,   -1 },
            [CP_HIGHLIGHT]    = { COLOR_CYAN,    -1 },
            [CP_BASH_COMMAND] = { COLOR_GREEN,   -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },

    /* ── 1: Monochrome ── */
    {
        .id   = "monochrome",
        .name = "Monochrome",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_WHITE,   -1 },
            [CP_HEADING_2]    = { COLOR_WHITE,   -1 },
            [CP_HEADING_3]    = { COLOR_WHITE,   -1 },
            [CP_HEADING_4]    = { COLOR_WHITE,   -1 },
            [CP_HEADING_5]    = { COLOR_WHITE,   -1 },
            [CP_HEADING_6]    = { COLOR_WHITE,   -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_WHITE,   -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLACK },
            [CP_BLOCKQUOTE]   = { COLOR_WHITE,   -1 },
            [CP_LIST_MARKER]  = { COLOR_WHITE,   -1 },
            [CP_LINK]         = { COLOR_WHITE,   -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_WHITE },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_WHITE,   -1 },
            [CP_HIGHLIGHT]    = { COLOR_WHITE,   -1 },
            [CP_BASH_COMMAND] = { COLOR_WHITE,   -1 },
            [CP_BASH_STRING]  = { COLOR_WHITE,   -1 },
            [CP_BASH_VARIABLE]= { COLOR_WHITE,   -1 },
            [CP_BASH_KEYWORD] = { COLOR_WHITE,   -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_WHITE,   -1 },
            [CP_KW_TYPE]     = { COLOR_WHITE,   -1 },
            [CP_KW_STRING]   = { COLOR_WHITE,   -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_WHITE,   -1 },
            [CP_KW_PREPROC]  = { COLOR_WHITE,   -1 },
        },
    },

    /* ── 2: Solarized Dark ── */
    {
        .id   = "solarized-dark",
        .name = "Solarized Dark",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_6]    = { COLOR_MAGENTA, -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_YELLOW,  -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLACK },
            [CP_BLOCKQUOTE]   = { COLOR_CYAN,    -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_CYAN },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_YELLOW,  -1 },
            [CP_HIGHLIGHT]    = { COLOR_CYAN,    -1 },
            [CP_BASH_COMMAND] = { COLOR_GREEN,   -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },

    /* ── 3: Solarized Light ── */
    {
        .id   = "solarized-light",
        .name = "Solarized Light",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_BLACK,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_6]    = { COLOR_MAGENTA, -1 },
            [CP_BOLD]         = { COLOR_BLACK,   -1 },
            [CP_ITALIC]       = { COLOR_BLACK,   -1 },
            [CP_CODE]         = { COLOR_YELLOW,  -1 },
            [CP_CODE_BLOCK]   = { COLOR_BLACK,   COLOR_WHITE },
            [CP_BLOCKQUOTE]   = { COLOR_CYAN,    -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_BLACK,   -1 },
            [CP_STATUSBAR]    = { COLOR_WHITE,   COLOR_BLUE },
            [CP_TABLE_BORDER] = { COLOR_BLACK,   -1 },
            [CP_TABLE_HEADER] = { COLOR_BLACK,   -1 },
            [CP_HIGHLIGHT]    = { COLOR_BLUE,    -1 },
            [CP_BASH_COMMAND] = { COLOR_BLUE,    -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_BLUE,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_BLACK,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_BLACK,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },

    /* ── 4: Nord ── */
    {
        .id   = "nord",
        .name = "Nord",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_6]    = { COLOR_MAGENTA, -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_GREEN,   -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLACK },
            [CP_BLOCKQUOTE]   = { COLOR_CYAN,    -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_CYAN },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_CYAN,    -1 },
            [CP_HIGHLIGHT]    = { COLOR_CYAN,    -1 },
            [CP_BASH_COMMAND] = { COLOR_GREEN,   -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },

    /* ── 5: Gruvbox Dark ── */
    {
        .id   = "gruvbox",
        .name = "Gruvbox Dark",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_6]    = { COLOR_MAGENTA, -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_GREEN,   -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLACK },
            [CP_BLOCKQUOTE]   = { COLOR_YELLOW,  -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_YELLOW },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_YELLOW,  -1 },
            [CP_HIGHLIGHT]    = { COLOR_YELLOW,  -1 },
            [CP_BASH_COMMAND] = { COLOR_GREEN,   -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },

    /* ── 6: Dracula ── */
    {
        .id   = "dracula",
        .name = "Dracula",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_MAGENTA, -1 },
            [CP_HEADING_6]    = { COLOR_BLUE,    -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_GREEN,   -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLACK },
            [CP_BLOCKQUOTE]   = { COLOR_MAGENTA, -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_CYAN,    -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_MAGENTA },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_CYAN,    -1 },
            [CP_HIGHLIGHT]    = { COLOR_CYAN,    -1 },
            [CP_BASH_COMMAND] = { COLOR_GREEN,   -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },

    /* ── 7: One Light (complemento de One Dark para fondos blancos) ── */
    {
        .id   = "one-light",
        .name = "One Light",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_BLACK,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_MAGENTA, -1 },
            [CP_HEADING_6]    = { COLOR_RED,     -1 },
            [CP_BOLD]         = { COLOR_BLACK,   -1 },
            [CP_ITALIC]       = { COLOR_BLACK,   -1 },
            [CP_CODE]         = { COLOR_MAGENTA, -1 },
            [CP_CODE_BLOCK]   = { COLOR_BLACK,   COLOR_WHITE },
            [CP_BLOCKQUOTE]   = { COLOR_BLUE,    -1 },
            [CP_LIST_MARKER]  = { COLOR_BLUE,    -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_BLACK,   -1 },
            [CP_STATUSBAR]    = { COLOR_WHITE,   COLOR_BLACK },
            [CP_TABLE_BORDER] = { COLOR_BLACK,   -1 },
            [CP_TABLE_HEADER] = { COLOR_BLACK,   -1 },
            [CP_HIGHLIGHT]    = { COLOR_MAGENTA, -1 },
            [CP_BASH_COMMAND] = { COLOR_BLUE,    -1 },
            [CP_BASH_STRING]  = { COLOR_MAGENTA, -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_RED,     -1 },
            [CP_LINE_NUMBER]  = { COLOR_BLACK,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_MAGENTA, -1 },
            [CP_KW_COMMENT]  = { COLOR_BLACK,   -1 },
            [CP_KW_NUMBER]   = { COLOR_RED,     -1 },
            [CP_KW_PREPROC]  = { COLOR_RED,     -1 },
        },
    },

    /* ── 8: One Dark ── */
    {
        .id   = "one-dark",
        .name = "One Dark",
        .pairs = {
            [CP_DEFAULT]      = { COLOR_WHITE,   -1 },
            [CP_HEADING_1]    = { COLOR_RED,     -1 },
            [CP_HEADING_2]    = { COLOR_YELLOW,  -1 },
            [CP_HEADING_3]    = { COLOR_GREEN,   -1 },
            [CP_HEADING_4]    = { COLOR_CYAN,    -1 },
            [CP_HEADING_5]    = { COLOR_BLUE,    -1 },
            [CP_HEADING_6]    = { COLOR_MAGENTA, -1 },
            [CP_BOLD]         = { COLOR_WHITE,   -1 },
            [CP_ITALIC]       = { COLOR_WHITE,   -1 },
            [CP_CODE]         = { COLOR_GREEN,   -1 },
            [CP_CODE_BLOCK]   = { COLOR_WHITE,   COLOR_BLACK },
            [CP_BLOCKQUOTE]   = { COLOR_CYAN,    -1 },
            [CP_LIST_MARKER]  = { COLOR_YELLOW,  -1 },
            [CP_LINK]         = { COLOR_BLUE,    -1 },
            [CP_HR]           = { COLOR_WHITE,   -1 },
            [CP_STATUSBAR]    = { COLOR_BLACK,   COLOR_CYAN },
            [CP_TABLE_BORDER] = { COLOR_WHITE,   -1 },
            [CP_TABLE_HEADER] = { COLOR_CYAN,    -1 },
            [CP_HIGHLIGHT]    = { COLOR_YELLOW,  -1 },
            [CP_BASH_COMMAND] = { COLOR_GREEN,   -1 },
            [CP_BASH_STRING]  = { COLOR_YELLOW,  -1 },
            [CP_BASH_VARIABLE]= { COLOR_CYAN,    -1 },
            [CP_BASH_KEYWORD] = { COLOR_MAGENTA, -1 },
            [CP_LINE_NUMBER]  = { COLOR_WHITE,   -1 },
            [CP_KW_KEYWORD]  = { COLOR_BLUE,    -1 },
            [CP_KW_TYPE]     = { COLOR_CYAN,    -1 },
            [CP_KW_STRING]   = { COLOR_YELLOW,  -1 },
            [CP_KW_COMMENT]  = { COLOR_WHITE,   -1 },
            [CP_KW_NUMBER]   = { COLOR_MAGENTA, -1 },
            [CP_KW_PREPROC]  = { COLOR_MAGENTA, -1 },
        },
    },
};

/* ══════════════════════════════════════════════════════════════
 * acceso a temas
 * ══════════════════════════════════════════════════════════════ */

int theme_get_count(void) {
    return (int)(sizeof(themes) / sizeof(themes[0]));
}

const Theme *theme_get_by_index(int idx) {
    int n = theme_get_count();
    if (idx < 0 || idx >= n) return &themes[0];
    return &themes[idx];
}

int theme_find_by_id(const char *id) {
    if (!id) return -1;
    int n = theme_get_count();
    for (int i = 0; i < n; i++) {
        if (strcmp(themes[i].id, id) == 0) return i;
    }
    return -1;
}

/* ══════════════════════════════════════════════════════════════
 * archivo de configuración
 * ══════════════════════════════════════════════════════════════ */

int config_get_path(char *buf, size_t bufsz) {
    const char *xdg  = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg && xdg[0]) {
        int n = snprintf(buf, bufsz, "%s/visormd/config", xdg);
        if (n < 0 || (size_t)n >= bufsz) return -1;
        return 0;
    }
    if (home && home[0]) {
        int n = snprintf(buf, bufsz, "%s/.config/visormd/config", home);
        if (n < 0 || (size_t)n >= bufsz) return -1;
        return 0;
    }
    return -1;  /* no hay dónde guardar */
}

int config_load_theme(void) {
    char path[1024];
    if (config_get_path(path, sizeof(path)) != 0) return 0;

    FILE *fp = fopen(path, "r");
    if (!fp) return 0;  /* archivo no existe → default */

    char line[256];
    int  idx = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* ignorar comentarios */
        if (line[0] == '#') continue;

        /* buscar "theme=" */
        char *eq = strchr(line, '=');
        if (!eq) continue;

        /* extraer clave */
        size_t keylen = (size_t)(eq - line);
        while (keylen > 0 &&
               (line[keylen - 1] == ' ' || line[keylen - 1] == '\t'))
            keylen--;

        if (keylen != 5 || strncmp(line, "theme", 5) != 0) continue;

        /* extraer valor (saltear espacios después del '=') */
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;

        /* quitar newline / espacios finales */
        size_t vlen = strlen(val);
        while (vlen > 0 &&
               (val[vlen - 1] == '\n' || val[vlen - 1] == '\r' ||
                val[vlen - 1] == ' '  || val[vlen - 1] == '\t'))
            val[--vlen] = '\0';

        if (vlen == 0) continue;

        int found = theme_find_by_id(val);
        if (found >= 0) idx = found;
    }

    fclose(fp);
    return idx;
}

/* ── crear directorios recursivamente (como mkdir -p) ── */
static int mkdir_p(const char *path, mode_t mode) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

int config_save_theme(const char *theme_id) {
    char path[1024];
    if (config_get_path(path, sizeof(path)) != 0) return -1;

    /* crear directorio si no existe (incluye padres) */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (mkdir_p(dir, 0755) != 0) return -1;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "theme=%s\n", theme_id ? theme_id : "default");
    fclose(fp);
    return 0;
}
