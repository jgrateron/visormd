#ifndef PARSER_H
#define PARSER_H

/* ── tipo de línea (determina el color base) ── */
typedef enum {
    LINE_PARAGRAPH,
    LINE_HEADING_1,
    LINE_HEADING_2,
    LINE_HEADING_3,
    LINE_HEADING_4,
    LINE_HEADING_5,
    LINE_HEADING_6,
    LINE_CODE_BLOCK,
    LINE_BLOCKQUOTE,
    LINE_LIST_UNORDERED,
    LINE_LIST_ORDERED,
    LINE_HORIZONTAL_RULE,
    LINE_EMPTY,
} LineType;

/* ── tipo de span (formato inline) ── */
typedef enum {
    SPAN_NORMAL,
    SPAN_BOLD,
    SPAN_ITALIC,
    SPAN_BOLD_ITALIC,
    SPAN_CODE,
    SPAN_LINK_TEXT,
    SPAN_LINK_URL,
    SPAN_LIST_MARKER,
} SpanType;

/* ── un fragmento de texto con un estilo uniforme ── */
typedef struct {
    char    *text;   /* contenido sin delimitadores md */
    SpanType type;
    char    *url;    /* solo para SPAN_LINK_TEXT/URL */
} Span;

/* ── una línea parseada: tipo + lista de spans ── */
typedef struct {
    Span    *spans;
    int      span_count;
    int      span_capacity;
    LineType type;
    int      indent; /* nivel de indentación (0..n) */
} ParsedLine;

/* ── documento completo parseado ── */
typedef struct {
    ParsedLine *lines;
    int         count;
    int         capacity;
} Document;

Document *doc_create(void);
void      doc_free(Document *doc);
int       doc_parse(Document *doc, char **raw_lines, int raw_count);

#endif
