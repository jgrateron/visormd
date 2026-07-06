#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static void flush_normal(CharBuf *buf, ParsedLine *line) {
    if (buf->len > 0) {
        char *text = cb_detach(buf);
        add_span(line, text, SPAN_NORMAL, NULL);
        free(text);
    }
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
                add_span(line, inner, SPAN_BOLD, NULL);
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
                add_span(line, inner, SPAN_ITALIC, NULL);
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
            cb_append(&buf, text[i++]);
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

/* ──────────────────────────────────────────────
 * extrae el marcador de una lista
 * ej: "- Tarea" → marcador="- ", resto="Tarea"
 * ────────────────────────────────────────────── */
static int extract_list_marker(const char *raw, LineType type,
                               char **marker, const char **rest) {
    const char *p = raw;
    while (*p == ' ') p++; /* saltar indentación */

    if (type == LINE_LIST_UNORDERED) {
        *marker = strndup(p, 2);   /* "- " | "* " | "+ " */
        *rest   = p + 2;
        return 1;
    }
    if (type == LINE_LIST_ORDERED) {
        const char *q = p;
        while (isdigit((unsigned char)*q)) q++;
        int ml = (int)(q - p) + 2; /* "123. " */
        *marker = strndup(p, (size_t)ml);
        *rest   = p + ml;
        return 1;
    }
    return 0;
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
    int in_code_block = 0;

    for (int i = 0; i < raw_count; i++) {
        ParsedLine line = {0};

        /* ── detección de cerca de código ── */
        if (is_code_fence(raw_lines[i])) {
            in_code_block = !in_code_block;
            line.type  = LINE_CODE_BLOCK;
            add_span(&line, raw_lines[i], SPAN_NORMAL, NULL);
            doc_add_line(doc, &line);
            continue;
        }

        /* ── dentro de un bloque de código ── */
        if (in_code_block) {
            line.type  = LINE_CODE_BLOCK;
            add_span(&line, raw_lines[i], SPAN_NORMAL, NULL);
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
