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
 * ancho visual de un span (en columnas de terminal)
 * ────────────────────────────────────────────── */
static int span_visual_width(Span *span) {
    wchar_t wbuf[4096];
    int wlen = utf8_to_wide(span->text, (int)strlen(span->text), wbuf, 4096);
    int total = 0;
    for (int j = 0; j < wlen; j++) {
        int cw = wcwidth(wbuf[j]);
        total += (cw < 0) ? 1 : cw;
    }
    return total;
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
 * helper: ancho visual de un rango de spans
 * ────────────────────────────────────────────── */
static int range_visual_width(Span *spans, int start, int end) {
    int total = 0;
    for (int s = start; s < end; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;
        total += span_visual_width(&spans[s]);
    }
    return total;
}

/* ──────────────────────────────────────────────
 * imprimir una porción horizontal de una celda
 * (salta skip_cols, dibuja hasta max_cols)
 * retorna columnas visuales dibujadas
 * ────────────────────────────────────────────── */
static int fprint_cell_slice(FILE *f, int use_ansi,
                              Span *spans, int start, int end,
                              int skip_cols, int max_cols,
                              LineType line_type) {
    int rendered = 0;
    int skipped  = 0;
    wchar_t wbuf[4096];

    for (int s = start; s < end && rendered < max_cols; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;

        Span *sp = &spans[s];
        int wlen = utf8_to_wide(sp->text, (int)strlen(sp->text),
                                wbuf, 4096);

        /* emitir prefijo ANSI para el span */
        int span_started = 0;

        for (int j = 0; j < wlen && rendered < max_cols; j++) {
            int cw = wcwidth(wbuf[j]);
            if (cw < 0) cw = 1;

            if (skipped < skip_cols) {
                skipped += cw;
                continue;
            }

            if (rendered + cw > max_cols)
                return rendered;

            if (!span_started) {
                emit_span_ansi(f, use_ansi, sp->type, line_type);
                span_started = 1;
            }

            /* imprimir el wide char como UTF-8 */
            char mb[MB_CUR_MAX + 1];
            int len = (int)wctomb(mb, wbuf[j]);
            if (len > 0) {
                mb[len] = '\0';
                fputs(mb, f);
            }
            rendered += cw;
        }

        if (span_started && use_ansi)
            fprintf(f, "%s", RST);
    }

    return rendered;
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

    /* calcular líneas de wrapping por celda */
    int *cell_cw    = malloc(sizeof(int) * (size_t)ncols);
    int *cell_lines = malloc(sizeof(int) * (size_t)ncols);
    int  max_lines  = 1;

    if (!cell_cw || !cell_lines) {
        free(cell_start); free(cell_end);
        free(cell_cw);  free(cell_lines);
        return;
    }

    for (int c = 0; c < ncols; c++) {
        cell_cw[c] = range_visual_width(line->spans,
                                         cell_start[c], cell_end[c]);
        cell_lines[c] = (cell_cw[c] > 0)
            ? ((cell_cw[c] + adj_w[c] - 1) / adj_w[c])
            : 1;
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

            if (wr < cell_lines[c] && cell_start[c] < cell_end[c]) {
                int skip_cols = wr * col_w;
                int render_w  = col_w;

                /* ajustar ancho en la última línea de la celda */
                if (wr == cell_lines[c] - 1) {
                    int remaining = cell_cw[c] - skip_cols;
                    if (remaining < render_w) render_w = remaining;
                }

                /* alineación para celdas de una sola línea */
                int align_pad = 0;
                if (cell_lines[c] == 1 && render_w < col_w &&
                    line->table_aligns) {
                    int pad = col_w - render_w;
                    if (line->table_aligns[c] == 0)      /* centro */
                        align_pad = pad / 2;
                    else if (line->table_aligns[c] == 1)  /* derecha */
                        align_pad = pad;
                    for (int p = 0; p < align_pad; p++)
                        fputc(' ', f);
                }

                int drawn = fprint_cell_slice(f, use_ansi,
                                               line->spans,
                                               cell_start[c], cell_end[c],
                                               skip_cols, render_w,
                                               line->type);

                /* rellenar espacio sobrante */
                int fill = col_w - drawn - align_pad;
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
    free(cell_cw);   free(cell_lines);
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
