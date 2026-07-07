#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>

/* ──────────────────────────────────────────────
 * buffer de caracteres dinámico (para acumular
 * texto normal entre spans con formato)
 * ────────────────────────────────────────────── */
typedef struct {
    char *data;
    int   len;
    int   cap;
} CharBuf;

static void cb_init(CharBuf *cb) {
    cb->data = NULL;
    cb->len  = 0;
    cb->cap  = 0;
}

static void cb_append(CharBuf *cb, char c) {
    if (cb->len >= cb->cap) {
        cb->cap = cb->cap ? cb->cap * 2 : 64;
        cb->data = realloc(cb->data, (size_t)cb->cap);
    }
    cb->data[cb->len++] = c;
}

static char *cb_detach(CharBuf *cb) {
    cb_append(cb, '\0');
    char *s = cb->data;
    cb->data = NULL;
    cb->len  = 0;
    cb->cap  = 0;
    return s;
}

/* ──────────────────────────────────────────────
 * helpers de spans
 * ────────────────────────────────────────────── */
static void add_span(ParsedLine *line, const char *text,
                     SpanType type, const char *url) {
    if (line->span_count >= line->span_capacity) {
        line->span_capacity = line->span_capacity ? line->span_capacity * 2 : 8;
        line->spans = realloc(line->spans,
                              sizeof(Span) * (size_t)line->span_capacity);
    }
    Span *s = &line->spans[line->span_count++];
    s->text = text ? strdup(text) : strdup("");
    s->type = type;
    s->url  = url  ? strdup(url)  : NULL;
}

static void flush_buf(CharBuf *buf, ParsedLine *line, SpanType type) {
    if (buf->len > 0) {
        char *text = cb_detach(buf);
        add_span(line, text, type, NULL);
        free(text);
    }
}

static void flush_normal(CharBuf *buf, ParsedLine *line) {
    flush_buf(buf, line, SPAN_NORMAL);
}

/* ──────────────────────────────────────────────
 * búsqueda de substrings con offset
 * ────────────────────────────────────────────── */
static int find_str(const char *hay, const char *ndl, int start) {
    const char *p = strstr(hay + start, ndl);
    return p ? (int)(p - hay) : -1;
}

/* busca un '*' que no sea parte de '**' */
static int find_single_star(const char *text, int start) {
    for (int i = start; text[i]; i++) {
        if (text[i] == '*' && text[i + 1] != '*') return i;
        if (text[i] == '*' && text[i + 1] == '*') i++;
    }
    return -1;
}

/* ──────────────────────────────────────────────
 * parser inline: **bold**  *italic*  `code`
 *                [text](url)
 * ────────────────────────────────────────────── */
static void parse_inline(ParsedLine *line, const char *text) {
    CharBuf buf;
    cb_init(&buf);
    int len = (int)strlen(text);
    int i   = 0;

    while (i < len) {
        /* ── bold **...** ── */
        if (text[i] == '*' && text[i + 1] == '*') {
            int end = find_str(text, "**", i + 2);
            if (end >= 0) {
                flush_normal(&buf, line);
                char *inner = strndup(text + i + 2, (size_t)(end - i - 2));
                ParsedLine tmp = {0};
                parse_inline(&tmp, inner);
                for (int s = 0; s < tmp.span_count; s++) {
                    Span *sp = &tmp.spans[s];
                    SpanType t = sp->type;
                    if (t == SPAN_NORMAL)       t = SPAN_BOLD;
                    else if (t == SPAN_CODE)    t = SPAN_BOLD_CODE;
                    else if (t == SPAN_ITALIC)  t = SPAN_BOLD_ITALIC;
                    add_span(line, sp->text, t, sp->url);
                    free(sp->text);
                    free(sp->url);
                }
                free(tmp.spans);
                free(inner);
                i = end + 2;
                continue;
            }
            cb_append(&buf, text[i++]);
            continue;
        }

        /* ── italic *...* (pero no **) ── */
        if (text[i] == '*') {
            int end = find_single_star(text, i + 1);
            if (end >= 0) {
                flush_normal(&buf, line);
                char *inner = strndup(text + i + 1, (size_t)(end - i - 1));
                ParsedLine tmp = {0};
                parse_inline(&tmp, inner);
                for (int s = 0; s < tmp.span_count; s++) {
                    Span *sp = &tmp.spans[s];
                    SpanType t = sp->type;
                    if (t == SPAN_NORMAL)       t = SPAN_ITALIC;
                    else if (t == SPAN_CODE)    t = SPAN_ITALIC_CODE;
                    else if (t == SPAN_BOLD)    t = SPAN_BOLD_ITALIC;
                    add_span(line, sp->text, t, sp->url);
                    free(sp->text);
                    free(sp->url);
                }
                free(tmp.spans);
                free(inner);
                i = end + 1;
                continue;
            }
            cb_append(&buf, text[i++]);
            continue;
        }

        /* ── inline code `...` ── */
        if (text[i] == '`') {
            int end = find_str(text, "`", i + 1);
            if (end >= 0) {
                flush_normal(&buf, line);
                char *inner = strndup(text + i + 1, (size_t)(end - i - 1));
                add_span(line, inner, SPAN_CODE, NULL);
                free(inner);
                i = end + 1;
                continue;
            }
            /* backtick solitario: el resto de la línea es código inline */
            flush_normal(&buf, line);
            add_span(line, text + i + 1, SPAN_CODE, NULL);
            i = len;
            continue;
        }

        /* ── link [text](url) ── */
        if (text[i] == '[') {
            int br = find_str(text, "]", i + 1);
            if (br >= 0 && text[br + 1] == '(') {
                int paren = find_str(text, ")", br + 2);
                if (paren >= 0) {
                    flush_normal(&buf, line);
                    char *lt = strndup(text + i + 1, (size_t)(br - i - 1));
                    char *lu = strndup(text + br + 2,(size_t)(paren - br - 2));
                    add_span(line, lt, SPAN_LINK_TEXT, lu);
                    free(lt);
                    free(lu);
                    i = paren + 1;
                    continue;
                }
            }
            cb_append(&buf, text[i++]);
            continue;
        }

        /* ── carácter normal ── */
        cb_append(&buf, text[i++]);
    }
    flush_normal(&buf, line);
}

/* ──────────────────────────────────────────────
 * detección del tipo de línea
 * ────────────────────────────────────────────── */
static LineType detect_line_type(const char *raw, char **out_text,
                                 int *out_indent) {
    *out_indent = 0;

    /* contar indentación con espacios */
    const char *p = raw;
    while (*p == ' ') { p++; (*out_indent)++; }

    if (*p == '\0') {
        *out_text = strdup("");
        return LINE_EMPTY;
    }

    /* ── cerca de bloque de código: ``` o ~~~ ── */
    if ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
        (p[0] == '~' && p[1] == '~' && p[2] == '~')) {
        *out_text = strdup(p);
        return LINE_CODE_BLOCK;
    }

    /* ── regla horizontal: --- *** ___ (3+ del mismo char) ── */
    if ((p[0] == '-' && p[1] == '-' && p[2] == '-') ||
        (p[0] == '*' && p[1] == '*' && p[2] == '*') ||
        (p[0] == '_' && p[1] == '_' && p[2] == '_')) {
        char hr_char = p[0];
        int  only_hr = 1;
        for (const char *c = p; *c; c++) {
            if (*c != hr_char && *c != ' ') { only_hr = 0; break; }
        }
        if (only_hr) {
            *out_text = strdup(p);
            return LINE_HORIZONTAL_RULE;
        }
    }

    /* ── encabezado: # a ###### ── */
    if (*p == '#') {
        int level = 0;
        while (*p == '#' && level < 6) { p++; level++; }
        if (*p == ' ') {
            p++;
            *out_text = strdup(p);
            return (LineType)(LINE_HEADING_1 + level - 1);
        }
    }

    /* ── cita: > texto ── */
    if (*p == '>') {
        p++;
        if (*p == ' ') p++;
        *out_text = strdup(p);
        return LINE_BLOCKQUOTE;
    }

    /* ── lista no ordenada: - * + ── */
    if ((p[0] == '-' || p[0] == '*' || p[0] == '+') && p[1] == ' ') {
        *out_text = strdup(raw);
        return LINE_LIST_UNORDERED;
    }

    /* ── lista ordenada: 1. 2. etc. ── */
    if (isdigit((unsigned char)*p)) {
        const char *q = p;
        while (isdigit((unsigned char)*q)) q++;
        if (*q == '.' && (q[1] == ' ' || q[1] == '\t')) {
            *out_text = strdup(raw);
            return LINE_LIST_ORDERED;
        }
    }

    /* ── párrafo ── */
    *out_text = strdup(raw);
    return LINE_PARAGRAPH;
}

/* ──────────────────────────────────────────────
 * helper: ¿es una línea de cerca de código?
 * ────────────────────────────────────────────── */
static int is_code_fence(const char *line) {
    while (*line == ' ') line++;
    return (line[0] == '`' && line[1] == '`' && line[2] == '`') ||
           (line[0] == '~' && line[1] == '~' && line[2] == '~');
}

/* ¿es una cerca de código sin especificador de lenguaje? */
static int is_bare_fence(const char *line) {
    while (*line == ' ') line++;
    if (!((line[0] == '`' && line[1] == '`' && line[2] == '`') ||
          (line[0] == '~' && line[1] == '~' && line[2] == '~')))
        return 0;
    line += 3;
    while (*line == ' ' || *line == '\t') line++;
    return (*line == '\0');
}

/* extrae el especificador de lenguaje de una cerca de código
 * ej: "```bash" → "bash", "```python" → "python", "```" → "" */
static void extract_fence_lang(const char *fence, char *lang, int lang_cap) {
    const char *p = fence;
    while (*p == ' ') p++;
    if ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
        (p[0] == '~' && p[1] == '~' && p[2] == '~'))
        p += 3;
    while (*p == ' ' || *p == '\t') p++;
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && i < lang_cap - 1)
        lang[i++] = *p++;
    lang[i] = '\0';
}

/* ──────────────────────────────────────────────
 * tokenizador bash: ¿es una palabra clave?
 * ────────────────────────────────────────────── */
static int is_bash_keyword(const char *word) {
    static const char *keywords[] = {
        "if", "then", "else", "elif", "fi",
        "for", "while", "do", "done", "case", "esac",
        "function", "in", "select", "until",
        NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

/* ──────────────────────────────────────────────
 * tokenizador bash: convierte una línea en spans
 * con resaltado de sintaxis (comandos, strings,
 * variables, keywords)
 * ────────────────────────────────────────────── */
static void parse_bash_line(ParsedLine *line, const char *text) {
    int len = (int)strlen(text);
    int i   = 0;
    int command_pos = 1;  /* inicio de línea → posición de comando */
    CharBuf buf;
    cb_init(&buf);

    while (i < len) {
        /* ── espacios: acumular como normal ── */
        if (text[i] == ' ' || text[i] == '\t') {
            cb_append(&buf, text[i++]);
            continue;
        }

        /* ── comentario: resto de línea sin color ── */
        if (text[i] == '#') {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            add_span(line, text + i, SPAN_BASH_NORMAL, NULL);
            return;
        }

        /* ── string con comillas dobles ── */
        if (text[i] == '"') {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            int start = i;
            i++;
            while (i < len && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            char *s = strndup(text + start, (size_t)(i - start));
            add_span(line, s, SPAN_BASH_STRING, NULL);
            free(s);
            command_pos = 0;
            continue;
        }

        /* ── string con comillas simples ── */
        if (text[i] == '\'') {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            int start = i;
            i++;
            while (i < len && text[i] != '\'') i++;
            if (i < len) i++;
            char *s = strndup(text + start, (size_t)(i - start));
            add_span(line, s, SPAN_BASH_STRING, NULL);
            free(s);
            command_pos = 0;
            continue;
        }

        /* ── variable: $VAR, ${VAR}, $1, $@, etc. ── */
        if (text[i] == '$' && text[i + 1] != '\0' && text[i + 1] != ' ' &&
            text[i + 1] != '\t') {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            int start = i;
            i++;  /* saltar $ */
            if (text[i] == '{') {
                i++;
                while (i < len && text[i] != '}') i++;
                if (i < len) i++;
            } else if (isdigit((unsigned char)text[i]) ||
                       text[i] == '@' || text[i] == '#' || text[i] == '?' ||
                       text[i] == '!' || text[i] == '-' || text[i] == '$' ||
                       text[i] == '*') {
                i++;
            } else {
                while (i < len && (isalnum((unsigned char)text[i]) ||
                                   text[i] == '_')) i++;
            }
            char *var = strndup(text + start, (size_t)(i - start));
            add_span(line, var, SPAN_BASH_VARIABLE, NULL);
            free(var);
            command_pos = 0;
            continue;
        }

        /* ── operadores que separan comandos ── */
        if ((text[i] == '|' && text[i + 1] == '|') ||
            (text[i] == '&' && text[i + 1] == '&')) {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            char op[3] = {text[i], text[i + 1], '\0'};
            add_span(line, op, SPAN_BASH_NORMAL, NULL);
            i += 2;
            command_pos = 1;
            continue;
        }
        if (text[i] == '|' || text[i] == ';') {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            char op[2] = {text[i], '\0'};
            add_span(line, op, SPAN_BASH_NORMAL, NULL);
            i++;
            command_pos = 1;
            continue;
        }

        /* ── palabra: comando, keyword, o argumento ── */
        if (isalnum((unsigned char)text[i]) || text[i] == '_' ||
            text[i] == '-' || text[i] == '/' || text[i] == '.') {
            flush_buf(&buf, line, SPAN_BASH_NORMAL);
            int start = i;
            while (i < len && (isalnum((unsigned char)text[i]) ||
                   text[i] == '_' || text[i] == '-' ||
                   text[i] == '/' || text[i] == '.')) {
                i++;
            }
            char *word = strndup(text + start, (size_t)(i - start));

            if (is_bash_keyword(word)) {
                add_span(line, word, SPAN_BASH_KEYWORD, NULL);
            } else if (command_pos) {
                add_span(line, word, SPAN_BASH_COMMAND, NULL);
            } else {
                add_span(line, word, SPAN_BASH_NORMAL, NULL);
            }
            free(word);
            command_pos = 0;
            continue;
        }

        /* ── cualquier otro carácter (operadores, etc.) ── */
        cb_append(&buf, text[i++]);
    }

    flush_buf(&buf, line, SPAN_BASH_NORMAL);
}

/* ──────────────────────────────────────────────
 * extrae el marcador de una lista
 * ej: "- Tarea" → marcador="- ", resto="Tarea"
 * ────────────────────────────────────────────── */
static int extract_list_marker(const char *raw, LineType type,
                               char **marker, const char **rest) {
    const char *p = raw;
    const char *indent_start = p;
    while (*p == ' ') p++; /* saltar indentación */
    int indent_len = (int)(p - indent_start);

    if (type == LINE_LIST_UNORDERED) {
        /* marker = indent + "- "|"* "|"+ " */
        *marker = malloc((size_t)(indent_len + 3));
        if (indent_len > 0) memcpy(*marker, indent_start, (size_t)indent_len);
        memcpy(*marker + indent_len, p, 2);
        (*marker)[indent_len + 2] = '\0';
        *rest   = p + 2;
        return 1;
    }
    if (type == LINE_LIST_ORDERED) {
        const char *q = p;
        while (isdigit((unsigned char)*q)) q++;
        int ml = (int)(q - p) + 2; /* "123. " */
        *marker = malloc((size_t)(indent_len + ml + 1));
        if (indent_len > 0) memcpy(*marker, indent_start, (size_t)indent_len);
        memcpy(*marker + indent_len, p, (size_t)ml);
        (*marker)[indent_len + ml] = '\0';
        *rest   = p + ml;
        return 1;
    }
    return 0;
}

/* ──────────────────────────────────────────────
 * helpers de tablas (pipe tables)
 * ────────────────────────────────────────────── */

/* forward declaration */
static void doc_add_line(Document *doc, ParsedLine *line);
static int  parse_table_block(Document *doc, char **raw_lines,
                               int start, int end);

/* ¿la línea es parte de una tabla pipe? */
static int is_table_line(const char *line) {
    const char *p = line;
    while (*p == ' ') p++;

    if (!strchr(p, '|')) return 0;

    /* ¿es una fila separadora? (solo | - : espacios) */
    int only_sep = 1;
    for (const char *c = p; *c; c++) {
        if (*c != '|' && *c != '-' && *c != ':' && *c != ' ') {
            only_sep = 0;
            break;
        }
    }
    if (only_sep) return 1;

    /* fila normal: debe empezar con | */
    return (*p == '|');
}

/* ancho visual de un string UTF-8 (en columnas de terminal) */
static int str_visual_width(const char *utf8) {
    int width = 0;
    mbstate_t state = {0};
    size_t rem = strlen(utf8);
    const char *p = utf8;

    while (rem > 0) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, p, rem, &state);
        if (n == 0) break;
        if (n == (size_t)-1 || n == (size_t)-2) {
            width++;
            p++; rem--;
            memset(&state, 0, sizeof(state));
            continue;
        }
        int cw = wcwidth(wc);
        width += (cw < 0) ? 1 : cw;
        p   += n;
        rem -= n;
    }
    return width;
}

/* dividir una línea de tabla en celdas (sin los pipes) */
static int split_table_cells(const char *raw, char ***out_cells) {
    const char *p = raw;
    while (*p == ' ') p++;
    if (*p == '|') p++;

    /* contar pipes restantes para estimar */
    int pipes = 0;
    for (const char *q = p; *q; q++) if (*q == '|') pipes++;

    int max_cells = pipes + 1;
    char **cells = malloc(sizeof(char *) * (size_t)max_cells);
    if (!cells) return 0;
    int n = 0;

    while (*p) {
        const char *start = p;
        while (*p && *p != '|') p++;
        const char *end = p;

        /* trim izquierdo */
        while (start < end && (*start == ' ' || *start == '\t')) start++;
        /* trim derecho */
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;

        cells[n++] = strndup(start, (size_t)(end - start));

        if (*p == '|') p++;
    }

    /* eliminar celdas vacías sobrantes del final */
    while (n > 0 && cells[n - 1][0] == '\0') {
        free(cells[n - 1]);
        n--;
    }

    *out_cells = cells;
    return n;
}

/* ¿es una celda separadora?  ej: ---  :---  :---:  ---: */
static int is_separator_cell(const char *cell) {
    if (!cell || !*cell) return 0;
    const char *p = cell;
    while (*p == ' ') p++;
    if (*p == ':') p++;
    int dashes = 0;
    while (*p == '-') { dashes++; p++; }
    if (dashes < 3) return 0;
    if (*p == ':') p++;
    while (*p == ' ') p++;
    return *p == '\0';
}

/* alineación desde celda separadora: -1=izq, 0=centro, 1=der */
static int cell_alignment(const char *cell) {
    if (!cell || !*cell) return -1;
    const char *p = cell;
    while (*p == ' ') p++;
    int left = 0, right = 0;
    if (*p == ':') { left = 1; p++; }
    while (*p == '-') p++;
    if (*p == ':') { right = 1; p++; }
    if (left && right) return  0;
    if (right)         return  1;
    return -1;
}

/* ──────────────────────────────────────────────
 * construir línea de borde box-drawing
 * style: 0=top(┌┬┐), 1=mid(├┼┤), 2=bottom(└┴┘)
 * ────────────────────────────────────────────── */
static char *build_border(int ncols, int *widths, int style) {
    const char *left, *mid, *right, *fill;
    fill  = "─";
    if (style == 0)      { left = "┌"; mid = "┬"; right = "┐"; }
    else if (style == 1) { left = "├"; mid = "┼"; right = "┤"; }
    else                 { left = "└"; mid = "┴"; right = "┘"; }

    size_t fl = strlen(fill), ll = strlen(left), ml = strlen(mid), rl = strlen(right);
    size_t total = ll + rl + 1; /* esquinas + \0 */
    for (int c = 0; c < ncols; c++) {
        total += (size_t)widths[c] * fl;
        if (c > 0) total += ml;
    }

    char *buf = malloc(total);
    if (!buf) return NULL;

    char *p = buf;
    memcpy(p, left, ll); p += ll;
    for (int c = 0; c < ncols; c++) {
        if (c > 0) { memcpy(p, mid, ml); p += ml; }
        for (int i = 0; i < widths[c]; i++) {
            memcpy(p, fill, fl); p += fl;
        }
    }
    memcpy(p, right, rl); p += rl;
    *p = '\0';

    return buf;
}

/* procesar un bloque contiguo de filas de tabla
 * retorna cantidad de líneas consumidas, 0 si no era tabla válida */
static int parse_table_block(Document *doc, char **raw_lines,
                               int start, int end) {
    int row_count = end - start;
    if (row_count < 2) return 0;  /* mínimo: encabezado + separador */

    int success = 0;

    /* ── fase 1: dividir todas las filas en celdas ── */
    char ***rows    = NULL;
    int   *cell_cnt = NULL;
    int   *widths   = NULL;
    int   *aligns   = NULL;
    ParsedLine *cell_parsed = NULL;

    rows    = malloc(sizeof(char **) * (size_t)row_count);
    cell_cnt = malloc(sizeof(int)    * (size_t)row_count);
    if (!rows || !cell_cnt) goto cleanup;

    for (int r = 0; r < row_count; r++) {
        cell_cnt[r] = split_table_cells(raw_lines[start + r], &rows[r]);
    }

    int ncols = cell_cnt[0];
    if (ncols < 1) goto cleanup;

    /* ── fase 2: encontrar fila separadora ── */
    int sep_row = -1;
    for (int r = 0; r < row_count; r++) {
        if (cell_cnt[r] < 1) continue;
        int all_sep = 1;
        for (int c = 0; c < cell_cnt[r]; c++) {
            if (!is_separator_cell(rows[r][c])) { all_sep = 0; break; }
        }
        if (all_sep) { sep_row = r; break; }
    }
    if (sep_row < 0) goto cleanup;

    /* ── fase 3: parsear alineación ── */
    aligns = malloc(sizeof(int) * (size_t)ncols);
    if (!aligns) goto cleanup;
    for (int c = 0; c < ncols && c < cell_cnt[sep_row]; c++) {
        aligns[c] = cell_alignment(rows[sep_row][c]);
    }
    /* rellenar columnas extra si las hay */
    for (int c = cell_cnt[sep_row]; c < ncols; c++) {
        aligns[c] = -1;
    }

    /* ── fase 4: parseo inline por celda ── */
    cell_parsed = calloc((size_t)(row_count * ncols), sizeof(ParsedLine));
    if (!cell_parsed) goto cleanup;

    for (int r = 0; r < row_count; r++) {
        if (r == sep_row) continue;
        for (int c = 0; c < ncols && c < cell_cnt[r]; c++) {
            ParsedLine *cl = &cell_parsed[r * ncols + c];
            parse_inline(cl, rows[r][c]);
        }
    }

    /* ── fase 5: calcular anchos máximos por columna ── */
    widths = malloc(sizeof(int) * (size_t)ncols);
    if (!widths) goto cleanup;
    for (int c = 0; c < ncols; c++) {
        widths[c] = 3;  /* ancho mínimo */
        for (int r = 0; r < row_count; r++) {
            if (r == sep_row) continue;
            ParsedLine *cl = &cell_parsed[r * ncols + c];
            int w = 0;
            for (int s = 0; s < cl->span_count; s++) {
                w += str_visual_width(cl->spans[s].text);
            }
            if (w > widths[c]) widths[c] = w;
        }
    }

    /* ── fase 6: emitir ParsedLines con bordes box-drawing ── */
    /* helper local */
    #define ADD_BORDER(sty, ltype) do { \
        ParsedLine bl; \
        memset(&bl, 0, sizeof(bl)); \
        bl.type = (ltype); \
        bl.table_cols = ncols; \
        bl.table_widths = malloc(sizeof(int) * (size_t)ncols); \
        bl.table_aligns = malloc(sizeof(int) * (size_t)ncols); \
        if (bl.table_widths && bl.table_aligns) { \
            memcpy(bl.table_widths, widths, sizeof(int) * (size_t)ncols); \
            memcpy(bl.table_aligns, aligns, sizeof(int) * (size_t)ncols); \
            char *btext = build_border(ncols, widths, (sty)); \
            if (btext) { add_span(&bl, btext, SPAN_NORMAL, NULL); free(btext); } \
            doc_add_line(doc, &bl); \
        } else { \
            free(bl.table_widths); free(bl.table_aligns); \
        } \
    } while(0)

    #define ADD_ROW(ridx, ltype, center) do { \
        ParsedLine rl; \
        memset(&rl, 0, sizeof(rl)); \
        rl.type = (ltype); \
        rl.table_cols = ncols; \
        rl.table_widths = malloc(sizeof(int) * (size_t)ncols); \
        rl.table_aligns = malloc(sizeof(int) * (size_t)ncols); \
        if (rl.table_widths && rl.table_aligns) { \
            memcpy(rl.table_widths, widths, sizeof(int) * (size_t)ncols); \
            for (int _c = 0; _c < ncols; _c++) \
                rl.table_aligns[_c] = (center) ? 0 : aligns[_c]; \
            for (int _c = 0; _c < ncols; _c++) { \
                add_span(&rl, "│", SPAN_TABLE_PIPE, NULL); \
                if (_c < cell_cnt[(ridx)]) { \
                    ParsedLine *cl = &cell_parsed[(ridx) * ncols + _c]; \
                    for (int _s = 0; _s < cl->span_count; _s++) { \
                        Span *sp = &cl->spans[_s]; \
                        add_span(&rl, sp->text, sp->type, sp->url); \
                    } \
                } \
            } \
            add_span(&rl, "│", SPAN_TABLE_PIPE, NULL); \
            doc_add_line(doc, &rl); \
        } else { \
            free(rl.table_widths); free(rl.table_aligns); \
        } \
    } while(0)

    ADD_BORDER(0, LINE_TABLE_TOP);   /* ┌──┬──┐ */

    for (int r = 0; r < row_count; r++) {
        if (r == sep_row) continue;
        int is_header = (sep_row < 0) ? (r == 0) : (r < sep_row);
        if (is_header)
            ADD_ROW(r, LINE_TABLE_HEADER, 1);  /* centrado */
    }

    ADD_BORDER(1, LINE_TABLE_HSEP);  /* ├──┼──┤ */

    for (int r = 0; r < row_count; r++) {
        if (r == sep_row) continue;
        int is_header = (sep_row < 0) ? (r == 0) : (r < sep_row);
        if (is_header) continue;
        /* ¿es el último dato? */
        int is_last = 1;
        for (int r2 = r + 1; r2 < row_count; r2++) {
            if (r2 == sep_row) continue;
            int h2 = (sep_row < 0) ? (r2 == 0) : (r2 < sep_row);
            if (!h2) { is_last = 0; break; }
        }
        ADD_ROW(r, LINE_TABLE_ROW, 0);  /* alineación markdown */
        if (is_last)
            ADD_BORDER(2, LINE_TABLE_BOTTOM);  /* └──┴──┘ */
        else
            ADD_BORDER(1, LINE_TABLE_RSEP);    /* ├──┼──┤ */
    }

    #undef ADD_BORDER
    #undef ADD_ROW

    success = 1;

cleanup:
    /* liberar recursos temporales */
    free(widths);
    free(aligns);

    if (cell_parsed) {
        for (int i = 0; i < row_count * ncols; i++) {
            ParsedLine *cl = &cell_parsed[i];
            for (int s = 0; s < cl->span_count; s++) {
                free(cl->spans[s].text);
                free(cl->spans[s].url);
            }
            free(cl->spans);
        }
        free(cell_parsed);
    }

    if (rows) {
        for (int r = 0; r < row_count; r++) {
            if (rows[r]) {
                for (int c = 0; c < cell_cnt[r]; c++) free(rows[r][c]);
                free(rows[r]);
            }
        }
        free(rows);
    }
    free(cell_cnt);
    return success ? row_count : 0;
}

/* ──────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────── */

Document *doc_create(void) {
    Document *doc = malloc(sizeof(Document));
    if (!doc) return NULL;
    doc->lines    = NULL;
    doc->count    = 0;
    doc->capacity = 0;
    return doc;
}

void doc_free(Document *doc) {
    if (!doc) return;
    for (int i = 0; i < doc->count; i++) {
        ParsedLine *line = &doc->lines[i];
        for (int j = 0; j < line->span_count; j++) {
            free(line->spans[j].text);
            free(line->spans[j].url);
        }
        free(line->spans);
        free(line->table_widths);
        free(line->table_aligns);
    }
    free(doc->lines);
    free(doc);
}

static void doc_add_line(Document *doc, ParsedLine *line) {
    if (doc->count >= doc->capacity) {
        doc->capacity = doc->capacity ? doc->capacity * 2 : 256;
        doc->lines = realloc(doc->lines,
                             sizeof(ParsedLine) * (size_t)doc->capacity);
    }
    doc->lines[doc->count++] = *line;
}

int doc_parse(Document *doc, char **raw_lines, int raw_count) {
    int  in_code_block = 0;
    int  in_highlight  = 0;
    char code_lang[32] = "";

    for (int i = 0; i < raw_count; i++) {
        ParsedLine line = {0};

        /* ── bloque de tabla (pipe table) ── */
        if (!in_code_block && !in_highlight && is_table_line(raw_lines[i])) {
            int start = i;
            while (i < raw_count && is_table_line(raw_lines[i])) i++;
            int consumed = parse_table_block(doc, raw_lines, start, i);
            if (consumed > 0) {
                i--;  /* el bucle incrementa, compensar */
                continue;
            }
            /* no era tabla válida, reintentar desde start como línea normal */
            i = start;
        }

        /* ── detección de cerca de código ── */
        if (is_code_fence(raw_lines[i])) {
            int bare = is_bare_fence(raw_lines[i]);

            if (in_code_block) {
                /* cualquier cerca cierra un bloque de código */
                in_code_block = 0;
                code_lang[0]  = '\0';
                /* no se emite la cerca de cierre */
            } else if (in_highlight) {
                /* cualquier cerca cierra un bloque resaltado */
                in_highlight = 0;
                /* no se emite la cerca de cierre */
            } else if (bare) {
                /* cerca sin lenguaje: abre bloque resaltado */
                in_highlight = 1;
                /* no se emite la cerca de apertura */
            } else {
                /* cerca con lenguaje: abre bloque de código */
                in_code_block = 1;
                extract_fence_lang(raw_lines[i], code_lang, sizeof(code_lang));
                /* no se emite la cerca de apertura */
            }
            continue;
        }

        /* ── dentro de un bloque resaltado ── */
        if (in_highlight) {
            line.type = LINE_HIGHLIGHT;
            parse_inline(&line, raw_lines[i]);
            doc_add_line(doc, &line);
            continue;
        }

        /* ── dentro de un bloque de código ── */
        if (in_code_block) {
            line.type = LINE_CODE_BLOCK;
            /* ¿bash / sh? tokenizar con resaltado */
            if (code_lang[0] &&
                (strcmp(code_lang, "bash") == 0 ||
                 strcmp(code_lang, "sh")   == 0 ||
                 strcmp(code_lang, "zsh")  == 0)) {
                parse_bash_line(&line, raw_lines[i]);
            } else {
                add_span(&line, raw_lines[i], SPAN_NORMAL, NULL);
            }
            doc_add_line(doc, &line);
            continue;
        }

        /* ── línea normal: detectar tipo ── */
        char *clean  = NULL;
        int   indent = 0;
        LineType lt  = detect_line_type(raw_lines[i], &clean, &indent);

        line.type   = lt;
        line.indent = indent;

        /* línea vacía o regla horizontal: un solo span */
        if (lt == LINE_EMPTY || lt == LINE_HORIZONTAL_RULE) {
            add_span(&line, clean, SPAN_NORMAL, NULL);
            free(clean);
            doc_add_line(doc, &line);
            continue;
        }

        /* lista: extraer marcador y parsear el resto */
        if (lt == LINE_LIST_UNORDERED || lt == LINE_LIST_ORDERED) {
            char       *marker = NULL;
            const char *rest   = NULL;
            if (extract_list_marker(raw_lines[i], lt, &marker, &rest)) {
                add_span(&line, marker, SPAN_LIST_MARKER, NULL);
                free(marker);
                parse_inline(&line, rest);
            } else {
                parse_inline(&line, clean);
            }
            free(clean);
            doc_add_line(doc, &line);
            continue;
        }

        /* párrafo, encabezado, cita, código: parse inline común */
        parse_inline(&line, clean);
        free(clean);
        doc_add_line(doc, &line);
    }

    return 0;
}
