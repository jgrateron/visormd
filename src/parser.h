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
    LINE_TABLE_HEADER,
    LINE_TABLE_ROW,
    LINE_TABLE_SEP,
    LINE_TABLE_TOP,
    LINE_TABLE_HSEP,
    LINE_TABLE_RSEP,
    LINE_TABLE_BOTTOM,
    LINE_HIGHLIGHT,
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
    SPAN_TABLE_PIPE,
    SPAN_BASH_COMMAND,
    SPAN_BASH_STRING,
    SPAN_BASH_VARIABLE,
    SPAN_BASH_KEYWORD,
    SPAN_BASH_NORMAL,
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
    int      indent;      /* nivel de indentación (0..n) */
    int      table_cols;  /* número de columnas (0 = no es tabla) */
    int     *table_widths; /* array[table_cols] ancho máximo por columna */
    int     *table_aligns; /* array[table_cols]: -1=izq, 0=centro, 1=der */
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
