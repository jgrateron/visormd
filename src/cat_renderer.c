#include "cat_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ──────────────────────────────────────────────
 * ANSI escape codes
 * ────────────────────────────────────────────── */
#define RST   "\033[0m"
#define BLD   "\033[1m"
#define ITL   "\033[3m"
#define UND   "\033[4m"
#define DIM   "\033[2m"

#define F_RED     "\033[31m"
#define F_GREEN   "\033[32m"
#define F_YELLOW  "\033[33m"
#define F_BLUE    "\033[34m"
#define F_MAGENTA "\033[35m"
#define F_CYAN    "\033[36m"
#define F_DEF     "\033[39m"

/* ──────────────────────────────────────────────
 * obtener ancho del terminal (fallback 80)
 * ────────────────────────────────────────────── */
static int get_term_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    const char *cols = getenv("COLUMNS");
    if (cols) {
        int w = atoi(cols);
        if (w > 0) return w;
    }
    return 80;
}

/* ──────────────────────────────────────────────
 * ¿stdout es un terminal? (para decidir si usar ANSI)
 * ────────────────────────────────────────────── */
static int stdout_is_tty(void) {
    return isatty(STDOUT_FILENO);
}

/* ──────────────────────────────────────────────
 * convertir UTF-8 → wchar_t[] (misma lógica que renderer.c)
 * ────────────────────────────────────────────── */
static int utf8_to_wide(const char *utf8, int byte_len,
                        wchar_t *wbuf, int wcap) {
    mbstate_t state = {0};
    const char *p   = utf8;
    size_t      rem = (size_t)byte_len;
    int         cnt = 0;

    while (rem > 0 && cnt < wcap) {
        wchar_t wc;
        size_t  n = mbrtowc(&wc, p, rem, &state);
        if (n == 0) break;
        if (n == (size_t)-1 || n == (size_t)-2) {
            wbuf[cnt++] = L'?';
            p++; rem--;
            memset(&state, 0, sizeof(state));
            continue;
        }
        wbuf[cnt++] = wc;
        p   += n;
        rem -= n;
    }
    return cnt;
}

/* ──────────────────────────────────────────────
 * mapeo de SpanType + LineType → prefijo ANSI
 * misma lógica que span_attr() en renderer.c
 * ────────────────────────────────────────────── */
static void emit_span_ansi(FILE *f, int use_ansi,
                           SpanType st, LineType lt) {
    if (!use_ansi) return;

    if (st == SPAN_TABLE_PIPE) {
        fprintf(f, "%s", DIM);
        return;
    }

    if (st == SPAN_NORMAL || st == SPAN_LIST_MARKER) {
        if (st == SPAN_LIST_MARKER) {
            fprintf(f, "%s", F_YELLOW);
        } else {
            switch (lt) {
            case LINE_HEADING_1:  fprintf(f, "%s%s", BLD, F_RED);     return;
            case LINE_HEADING_2:  fprintf(f, "%s%s", BLD, F_YELLOW);  return;
            case LINE_HEADING_3:  fprintf(f, "%s%s", BLD, F_GREEN);   return;
            case LINE_HEADING_4:  fprintf(f, "%s%s", BLD, F_BLUE);    return;
            case LINE_HEADING_5:  fprintf(f, "%s%s", BLD, F_MAGENTA); return;
            case LINE_HEADING_6:  fprintf(f, "%s%s", BLD, F_CYAN);    return;
            case LINE_CODE_BLOCK: fprintf(f, "%s",   F_DEF);          return;
            case LINE_BLOCKQUOTE: fprintf(f, "%s",   DIM);            return;
            case LINE_TABLE_HEADER: fprintf(f, "%s", BLD);            return;
            case LINE_HIGHLIGHT:  fprintf(f, "%s%s", BLD, F_YELLOW);  return;
            default:              fprintf(f, "%s",   F_DEF);          return;
            }
        }
        return;
    }

    /* spans con estilo propio */
    switch (st) {
    case SPAN_BOLD:         fprintf(f, "%s", BLD);               return;
    case SPAN_ITALIC:       fprintf(f, "%s", ITL);               return;
    case SPAN_BOLD_ITALIC:  fprintf(f, "%s%s", BLD, ITL);        return;
    case SPAN_CODE:         fprintf(f, "%s", F_YELLOW);          return;
    case SPAN_BOLD_CODE:    fprintf(f, "%s%s", BLD, F_YELLOW);   return;
    case SPAN_ITALIC_CODE:  fprintf(f, "%s%s", ITL, F_YELLOW);   return;
    case SPAN_LINK_TEXT:    fprintf(f, "%s%s", UND, F_BLUE);     return;
    case SPAN_LINK_URL:     fprintf(f, "%s%s", DIM, F_BLUE);     return;
    case SPAN_BASH_COMMAND: fprintf(f, "%s%s", BLD, F_GREEN);    return;
    case SPAN_BASH_STRING:  fprintf(f, "%s", F_YELLOW);          return;
    case SPAN_BASH_VARIABLE:fprintf(f, "%s", F_CYAN);            return;
    case SPAN_BASH_KEYWORD: fprintf(f, "%s%s", BLD, F_MAGENTA);  return;
    case SPAN_BASH_NORMAL:  fprintf(f, "%s", F_DEF);             return;
    case SPAN_KW_KEYWORD: fprintf(f, "%s%s", BLD, F_BLUE);     return;
    case SPAN_KW_TYPE:    fprintf(f, "%s", F_CYAN);            return;
    case SPAN_KW_STRING:  fprintf(f, "%s", F_YELLOW);          return;
    case SPAN_KW_COMMENT: fprintf(f, "%s%s", DIM, F_DEF);      return;
    case SPAN_KW_NUMBER:  fprintf(f, "%s", F_MAGENTA);         return;
    case SPAN_KW_PREPROC: fprintf(f, "%s", F_MAGENTA);         return;
    case SPAN_KW_NORMAL:  fprintf(f, "%s", F_DEF);             return;
    default:                return;
    }
}

/* ──────────────────────────────────────────────
 * imprimir un span completo con códigos ANSI
 * ────────────────────────────────────────────── */
static void fprint_span(FILE *f, int use_ansi,
                        Span *span, LineType line_type) {
    emit_span_ansi(f, use_ansi, span->type, line_type);
    fputs(span->text, f);
    if (use_ansi) fprintf(f, "%s", RST);

    /* para SPAN_LINK_TEXT con URL, mostrarla entre paréntesis */
    if (span->type == SPAN_LINK_TEXT && span->url && span->url[0]) {
        if (use_ansi)
            fprintf(f, "%s%s(%s)%s", DIM, F_BLUE, span->url, RST);
        else
            fprintf(f, "(%s)", span->url);
    }
}

/* ──────────────────────────────────────────────
 * aplanar un rango de spans (celda de tabla) en
 * arrays de wide chars + SpanType por carácter.
 * ────────────────────────────────────────────── */
static int flatten_cell_cat(Span *spans, int start, int end,
                             wchar_t **out_chars, SpanType **out_types) {
    wchar_t wbuf[4096];
    int total = 0;

    for (int s = start; s < end; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;
        total += utf8_to_wide(spans[s].text,
                              (int)strlen(spans[s].text), wbuf, 4096);
    }

    if (total == 0) {
        *out_chars = NULL;
        *out_types = NULL;
        return 0;
    }

    wchar_t   *chars = malloc(sizeof(wchar_t)   * (size_t)total);
    SpanType  *types = malloc(sizeof(SpanType)   * (size_t)total);
    if (!chars || !types) {
        free(chars); free(types);
        return -1;
    }

    int idx = 0;
    for (int s = start; s < end && idx < total; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;
        Span     *span = &spans[s];
        SpanType  st   = span->type;
        int wlen = utf8_to_wide(span->text, (int)strlen(span->text),
                                wbuf, 4096);
        for (int j = 0; j < wlen && idx < total; j++) {
            chars[idx] = wbuf[j];
            types[idx] = st;
            idx++;
        }
    }

    *out_chars = chars;
    *out_types = types;
    return total;
}

/* ──────────────────────────────────────────────
 * construir líneas visuales con word-wrap.
 * misma lógica que build_visual_lines en renderer.c
 * ────────────────────────────────────────────── */
static int build_visual_lines_cat(wchar_t *chars, int total, int avail_w,
                                   int **out_breaks) {
    if (total <= 0 || avail_w < 1) {
        *out_breaks = NULL;
        return (total <= 0) ? 0 : 1;
    }

    int *breaks = malloc(sizeof(int) * (size_t)(total + 1));
    if (!breaks) return -1;

    int pos = 0, line_count = 0;

    while (pos < total) {
        breaks[line_count++] = pos;

        int col = 0;
        int last_space_pos = -1;
        int line_end = pos;

        while (line_end < total) {
            int cw = wcwidth(chars[line_end]);
            if (cw < 0) cw = 1;
            if (col + cw > avail_w) break;
            if (chars[line_end] == L' ')
                last_space_pos = line_end;
            col += cw;
            line_end++;
        }

        if (line_end < total && last_space_pos >= pos)
            line_end = last_space_pos;

        if (line_end == pos) line_end = pos + 1;

        pos = line_end;
        if (pos < total && chars[pos] == L' ')
            pos++;
    }

    *out_breaks = breaks;
    return line_count;
}

/* ──────────────────────────────────────────────
 * dibujar borde de tabla (top, hsep, rsep, bottom)
 * ────────────────────────────────────────────── */
static void cat_render_table_border(FILE *f, int use_ansi,
                                     ParsedLine *line,
                                     int *adj_w, int ncols) {
    (void)line;
    const char *left, *mid, *right;
    switch (line->type) {
    case LINE_TABLE_TOP:    left = "┌"; mid = "┬"; right = "┐"; break;
    case LINE_TABLE_BOTTOM: left = "└"; mid = "┴"; right = "┘"; break;
    default:                left = "├"; mid = "┼"; right = "┤"; break;
    }

    if (use_ansi) fprintf(f, "%s", DIM);
    fputs(left, f);

    for (int c = 0; c < ncols; c++) {
        for (int i = 0; i < adj_w[c]; i++)
            fputs("─", f);
        if (c < ncols - 1)
            fputs(mid, f);
    }

    fputs(right, f);
    if (use_ansi) fprintf(f, "%s", RST);
    fputc('\n', f);
}

/* ──────────────────────────────────────────────
 * dibujar fila de tabla (header o datos) con wrapping
 * ────────────────────────────────────────────── */
static void cat_render_table_row(FILE *f, int use_ansi,
                                  ParsedLine *line,
                                  int *adj_w, int ncols) {
    /* encontrar rangos de spans por columna */
    int *cell_start = calloc((size_t)ncols, sizeof(int));
    int *cell_end   = calloc((size_t)ncols, sizeof(int));
    if (!cell_start || !cell_end) {
        free(cell_start); free(cell_end);
        return;
    }

    int ci = -1;
    for (int s = 0; s < line->span_count; s++) {
        if (line->spans[s].type == SPAN_TABLE_PIPE) {
            if (ci >= 0 && ci < ncols) cell_end[ci] = s;
            ci++;
            if (ci >= 0 && ci < ncols) cell_start[ci] = s + 1;
        }
    }
    if (ci >= 0 && ci < ncols) cell_end[ci] = line->span_count;

    /* ── preparar datos de celda con word-wrap ── */
    wchar_t  **cell_chars  = calloc((size_t)ncols, sizeof(wchar_t*));
    SpanType **cell_types  = calloc((size_t)ncols, sizeof(SpanType*));
    int      **cell_breaks = calloc((size_t)ncols, sizeof(int*));
    int       *cell_total  = calloc((size_t)ncols, sizeof(int));
    int       *cell_lines  = calloc((size_t)ncols, sizeof(int));
    int        max_lines   = 1;

    for (int c = 0; c < ncols; c++) {
        if (cell_start[c] < cell_end[c]) {
            cell_total[c] = flatten_cell_cat(line->spans,
                                              cell_start[c], cell_end[c],
                                              &cell_chars[c], &cell_types[c]);
            if (cell_total[c] > 0) {
                int *breaks = NULL;
                cell_lines[c] = build_visual_lines_cat(
                    cell_chars[c], cell_total[c], adj_w[c], &breaks);
                cell_breaks[c] = breaks;
                if (cell_lines[c] < 1) cell_lines[c] = 1;
            } else {
                cell_lines[c] = 1;
            }
        } else {
            cell_lines[c] = 1;
        }
        if (cell_lines[c] > max_lines) max_lines = cell_lines[c];
    }
    if (max_lines < 1) max_lines = 1;

    /* dibujar cada sub-fila */
    for (int wr = 0; wr < max_lines; wr++) {
        /* borde izquierdo */
        if (use_ansi) fprintf(f, "%s", DIM);
        fputs("│", f);
        if (use_ansi) fprintf(f, "%s", RST);

        for (int c = 0; c < ncols; c++) {
            int col_w = adj_w[c];

            if (wr < cell_lines[c] && cell_chars[c] && cell_breaks[c]) {
                int line_start = cell_breaks[c][wr];
                int line_end   = (wr + 1 < cell_lines[c])
                                 ? cell_breaks[c][wr + 1]
                                 : cell_total[c];

                /* ancho visual de esta línea */
                int line_width = 0;
                for (int k = line_start; k < line_end; k++) {
                    int cw = wcwidth(cell_chars[c][k]);
                    line_width += (cw < 0) ? 1 : cw;
                }

                /* alineación para celdas de una sola línea */
                int align_pad = 0;
                if (cell_lines[c] == 1 && line_width < col_w &&
                    line->table_aligns) {
                    int pad = col_w - line_width;
                    if (line->table_aligns[c] == 0)
                        align_pad = pad / 2;
                    else if (line->table_aligns[c] == 1)
                        align_pad = pad;
                    for (int p = 0; p < align_pad; p++)
                        fputc(' ', f);
                }

                /* dibujar caracteres agrupando mismo SpanType */
                int ci = line_start;
                while (ci < line_end) {
                    int run_end = ci + 1;
                    while (run_end < line_end &&
                           cell_types[c][run_end] == cell_types[c][ci])
                        run_end++;

                    emit_span_ansi(f, use_ansi, cell_types[c][ci],
                                   line->type);

                    for (int k = ci; k < run_end; k++) {
                        char mb[MB_CUR_MAX + 1];
                        int len = (int)wctomb(mb, cell_chars[c][k]);
                        if (len > 0) {
                            mb[len] = '\0';
                            fputs(mb, f);
                        }
                    }

                    if (use_ansi) fprintf(f, "%s", RST);
                    ci = run_end;
                }

                /* rellenar espacio sobrante */
                int fill = col_w - line_width - align_pad;
                if (fill < 0) fill = 0;
                for (int p = 0; p < fill; p++)
                    fputc(' ', f);
            } else {
                /* celda vacía en esta sub-fila */
                for (int p = 0; p < col_w; p++)
                    fputc(' ', f);
            }

            /* separador entre columnas */
            if (c < ncols - 1) {
                if (use_ansi) fprintf(f, "%s", DIM);
                fputs("│", f);
                if (use_ansi) fprintf(f, "%s", RST);
            }
        }

        /* borde derecho */
        if (use_ansi) fprintf(f, "%s", DIM);
        fputs("│", f);
        if (use_ansi) fprintf(f, "%s", RST);
        fputc('\n', f);
    }

    free(cell_start); free(cell_end);
    for (int c = 0; c < ncols; c++) {
        free(cell_chars[c]);
        free(cell_types[c]);
        free(cell_breaks[c]);
    }
    free(cell_chars); free(cell_types);
    free(cell_breaks); free(cell_total);
    free(cell_lines);
}

/* ──────────────────────────────────────────────
 * calcular anchos ajustados de columnas de tabla
 * ────────────────────────────────────────────── */
static int *table_adj_widths(ParsedLine *line, int avail_w, int ncols) {
    int pipes_w   = ncols + 1;
    int cell_avail = avail_w - pipes_w;
    if (cell_avail < ncols * 3) cell_avail = ncols * 3;

    int total_req = 0;
    for (int c = 0; c < ncols; c++)
        total_req += line->table_widths[c];

    int *adj_w = malloc(sizeof(int) * (size_t)ncols);
    if (!adj_w) return NULL;

    if (total_req > cell_avail) {
        int alloc = 0;
        for (int c = 0; c < ncols; c++) {
            adj_w[c] = line->table_widths[c] * cell_avail / total_req;
            if (adj_w[c] < 3) adj_w[c] = 3;
            alloc += adj_w[c];
        }
        int rem = cell_avail - alloc;
        for (int c = 0; c < ncols && rem > 0; c++) {
            adj_w[c]++; rem--;
        }
    } else {
        for (int c = 0; c < ncols; c++)
            adj_w[c] = line->table_widths[c];
    }

    return adj_w;
}

/* ──────────────────────────────────────────────
 * renderizar una línea parseada a stdout
 * ────────────────────────────────────────────── */
static void cat_render_line(FILE *f, int use_ansi,
                             ParsedLine *line, int term_w) {
    switch (line->type) {

    /* ── línea vacía ── */
    case LINE_EMPTY:
        fputc('\n', f);
        return;

    /* ── regla horizontal ── */
    case LINE_HORIZONTAL_RULE:
        if (use_ansi) fprintf(f, "%s", DIM);
        for (int x = 0; x < term_w; x++)
            fputs("─", f);
        if (use_ansi) fprintf(f, "%s", RST);
        fputc('\n', f);
        return;

    /* ── bordes de tabla ── */
    case LINE_TABLE_TOP:
    case LINE_TABLE_HSEP:
    case LINE_TABLE_RSEP:
    case LINE_TABLE_BOTTOM: {
        int ncols = line->table_cols;
        if (ncols <= 0 || !line->table_widths) { fputc('\n', f); return; }
        int *adj_w = table_adj_widths(line, term_w, ncols);
        if (!adj_w) { fputc('\n', f); return; }
        cat_render_table_border(f, use_ansi, line, adj_w, ncols);
        free(adj_w);
        return;
    }

    /* ── filas de tabla (header / datos) ── */
    case LINE_TABLE_HEADER:
    case LINE_TABLE_ROW: {
        int ncols = line->table_cols;
        if (ncols <= 0 || !line->table_widths) { fputc('\n', f); return; }
        int *adj_w = table_adj_widths(line, term_w, ncols);
        if (!adj_w) { fputc('\n', f); return; }
        cat_render_table_row(f, use_ansi, line, adj_w, ncols);
        free(adj_w);
        return;
    }

    /* ── separador de tabla (compatibilidad) ── */
    case LINE_TABLE_SEP:
        if (use_ansi) fprintf(f, "%s", DIM);
        for (int x = 0; x < term_w; x++)
            fputs("─", f);
        if (use_ansi) fprintf(f, "%s", RST);
        fputc('\n', f);
        return;

    /* ── todos los demás tipos (párrafo, headings, código, citas, listas, highlight) ── */
    default: {
        for (int s = 0; s < line->span_count; s++) {
            fprint_span(f, use_ansi, &line->spans[s], line->type);
        }
        fputc('\n', f);
        return;
    }
    }
}

/* ──────────────────────────────────────────────
 * punto de entrada público
 * ────────────────────────────────────────────── */
void cat_render(Document *doc) {
    if (!doc) return;

    int term_w = get_term_width();
    int use_ansi = stdout_is_tty();

    for (int i = 0; i < doc->count; i++) {
        cat_render_line(stdout, use_ansi, &doc->lines[i], term_w);
    }
}
