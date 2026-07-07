#include "renderer.h"
#include "theme.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ──────────────────────────────────────────────
 * convertir UTF-8 → wchar_t[] (requiere locale UTF-8)
 * retorna cantidad de wide chars
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
            wbuf[cnt++] = L'?'; /* inválido → ? */
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
 * pares de color (definidos en theme.h)
 * ────────────────────────────────────────────── */

struct Renderer {
    Document   *doc;
    char       *filename;

    /* ventanas ncurses */
    WINDOW     *main_win;
    WINDOW     *status_win;

    /* dimensiones */
    int         term_w;
    int         term_h;
    int         content_h;   /* term_h - 1 (barra de estado) */

    /* scroll */
    int         scroll_line;    /* índice de línea fuente en el tope */
    int         scroll_skip;    /* filas wrappeadas a saltar de scroll_line */
    int         anchor_line;    /* línea donde empezó el último scroll_page */
    int         anchor_skip;

    /* configuración */
    int         show_numbers;   /* toggle con 'n' */
    int         wrap_words;     /* toggle con 'w' */
    int         theme_idx;      /* índice en themes[] */
};

/* ──────────────────────────────────────────────
 * inicialización de colores (según tema)
 * ────────────────────────────────────────────── */
static void init_colors(int theme_idx) {
    use_default_colors();
    const Theme *t = theme_get_by_index(theme_idx);
    if (!t) return;

    for (int cp = CP_FIRST; cp < CP_COUNT; cp++) {
        init_pair(cp, t->pairs[cp].fg, t->pairs[cp].bg);
    }
}

/* ──────────────────────────────────────────────
 * mapeo de SpanType + LineType → atributos ncurses
 * ────────────────────────────────────────────── */
static chtype span_attr(SpanType st, LineType lt) {
    int    cp = CP_DEFAULT;
    chtype at = 0;

    /* SPAN_TABLE_PIPE siempre usa color de borde */
    if (st == SPAN_TABLE_PIPE) {
        return COLOR_PAIR(CP_TABLE_BORDER);
    }

    /* SPAN_NORMAL hereda el color del tipo de línea */
    if (st == SPAN_NORMAL || st == SPAN_LIST_MARKER) {
        if (st == SPAN_LIST_MARKER) { cp = CP_LIST_MARKER; }
        else {
            switch (lt) {
            case LINE_HEADING_1:     cp = CP_HEADING_1;  at = A_BOLD;   break;
            case LINE_HEADING_2:     cp = CP_HEADING_2;  at = A_BOLD;   break;
            case LINE_HEADING_3:     cp = CP_HEADING_3;  at = A_BOLD;   break;
            case LINE_HEADING_4:     cp = CP_HEADING_4;                 break;
            case LINE_HEADING_5:     cp = CP_HEADING_5;                 break;
            case LINE_HEADING_6:     cp = CP_HEADING_6;                 break;
            case LINE_CODE_BLOCK:    cp = CP_CODE_BLOCK;                break;
            case LINE_BLOCKQUOTE:    cp = CP_BLOCKQUOTE;                break;
            case LINE_TABLE_HEADER:  cp = CP_TABLE_HEADER; at = A_BOLD; break;
            case LINE_TABLE_ROW:     cp = CP_DEFAULT;                   break;
            case LINE_TABLE_SEP:     cp = CP_TABLE_BORDER;              break;
            case LINE_TABLE_TOP:     cp = CP_TABLE_BORDER;              break;
            case LINE_TABLE_HSEP:    cp = CP_TABLE_BORDER;              break;
            case LINE_TABLE_RSEP:    cp = CP_TABLE_BORDER;              break;
            case LINE_TABLE_BOTTOM:  cp = CP_TABLE_BORDER;              break;
            case LINE_HIGHLIGHT:     cp = CP_HIGHLIGHT;                 break;
            default:                 cp = CP_DEFAULT;                   break;
            }
        }
    } else {
        switch (st) {
        case SPAN_BOLD:        cp = CP_BOLD;   at = A_BOLD;        break;
        case SPAN_ITALIC:      cp = CP_ITALIC; at = A_ITALIC;      break;
        case SPAN_BOLD_ITALIC: cp = CP_BOLD;   at = A_BOLD|A_ITALIC;break;
        case SPAN_CODE:        cp = CP_CODE;                       break;
        case SPAN_BOLD_CODE:   cp = CP_CODE; at = A_BOLD;           break;
        case SPAN_ITALIC_CODE: cp = CP_CODE; at = A_ITALIC;         break;
        case SPAN_LINK_TEXT:   cp = CP_LINK;   at = A_UNDERLINE;   break;
        case SPAN_LINK_URL:    cp = CP_LINK;                       break;
        case SPAN_BASH_COMMAND:cp = CP_BASH_COMMAND; at = A_BOLD;  break;
        case SPAN_BASH_STRING: cp = CP_BASH_STRING;                break;
        case SPAN_BASH_VARIABLE:cp= CP_BASH_VARIABLE;              break;
        case SPAN_BASH_KEYWORD:cp = CP_BASH_KEYWORD; at = A_BOLD;  break;
        case SPAN_BASH_NORMAL:cp = CP_DEFAULT;                    break;
        default:               cp = CP_DEFAULT;                    break;
        }
    }
    return COLOR_PAIR(cp) | at;
}

/* ──────────────────────────────────────────────
 * helper: ancho visual total de los spans de una
 * celda (entre start y end, sin contar PIPEs)
 * ────────────────────────────────────────────── */
static int cell_spans_width(Span *spans, int start, int end) {
    int total = 0;
    wchar_t wbuf[4096];
    for (int s = start; s < end; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;
        int wlen = utf8_to_wide(spans[s].text,
                                (int)strlen(spans[s].text),
                                wbuf, 4096);
        for (int j = 0; j < wlen; j++) {
            int cw = wcwidth(wbuf[j]);
            total += (cw < 0) ? 1 : cw;
        }
    }
    return total;
}

/* ──────────────────────────────────────────────
 * helper: renderizar una porción horizontal de
 * una celda (salta skip_cols columnas visuales,
 * dibuja hasta max_cols). Preserva formato inline.
 * Retorna columnas visuales dibujadas.
 * Si screen_y < 0 solo mide, no dibuja.
 * ────────────────────────────────────────────── */
static int render_cell_slice(WINDOW *win, int screen_y, int screen_x,
                              Span *spans, int start, int end,
                              int skip_cols, int max_cols,
                              LineType line_type) {
    int rendered = 0;
    int skipped  = 0;
    wchar_t wbuf[4096];

    for (int s = start; s < end && rendered < max_cols; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;

        Span  *sp   = &spans[s];
        chtype attr = span_attr(sp->type, line_type);
        int    wlen = utf8_to_wide(sp->text, (int)strlen(sp->text),
                                   wbuf, 4096);

        for (int j = 0; j < wlen && rendered < max_cols; j++) {
            int cw = wcwidth(wbuf[j]);
            if (cw < 0) cw = 1;

            if (skipped < skip_cols) {
                skipped += cw;
                continue;
            }

            if (rendered + cw > max_cols)
                return rendered;

            if (screen_y >= 0) {
                wmove(win, screen_y, screen_x + rendered);
                wattron(win, attr);
                waddnwstr(win, wbuf + j, 1);
                wattroff(win, attr);
            }
            rendered += cw;
        }
    }

    return rendered;
}

/* ──────────────────────────────────────────────
 * aplanar todos los spans de una línea en arrays
 * de wide chars y atributos ncurses.
 * retorna cantidad de chars, 0 si vacío, -1 en error.
 * el caller debe liberar *out_chars y *out_attrs.
 * ────────────────────────────────────────────── */
/* ──────────────────────────────────────────────
 * aplanar un rango de spans (start..end-1) en arrays
 * de wide chars y atributos. usado para celdas de tabla.
 * ────────────────────────────────────────────── */
static int flatten_span_range(Span *spans, int start, int end,
                               LineType line_type,
                               wchar_t **out_chars, chtype **out_attrs) {
    wchar_t wbuf[4096];
    int total = 0;

    for (int s = start; s < end; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;
        total += utf8_to_wide(spans[s].text,
                              (int)strlen(spans[s].text), wbuf, 4096);
    }

    if (total == 0) {
        *out_chars = NULL;
        *out_attrs = NULL;
        return 0;
    }

    wchar_t *chars = malloc(sizeof(wchar_t) * (size_t)total);
    chtype  *attrs = malloc(sizeof(chtype)  * (size_t)total);
    if (!chars || !attrs) {
        free(chars);
        free(attrs);
        return -1;
    }

    int idx = 0;
    for (int s = start; s < end && idx < total; s++) {
        if (spans[s].type == SPAN_TABLE_PIPE) continue;
        Span  *span = &spans[s];
        chtype attr = span_attr(span->type, line_type);
        int    wlen = utf8_to_wide(span->text, (int)strlen(span->text),
                                    wbuf, 4096);
        for (int j = 0; j < wlen && idx < total; j++) {
            chars[idx] = wbuf[j];
            attrs[idx] = attr;
            idx++;
        }
    }

    *out_chars = chars;
    *out_attrs = attrs;
    return total;
}

static int flatten_spans(ParsedLine *line, wchar_t **out_chars,
                          chtype **out_attrs) {
    return flatten_span_range(line->spans, 0, line->span_count,
                               line->type, out_chars, out_attrs);
}

/* ──────────────────────────────────────────────
 * ancho visual de los marcadores de lista
 * (SPAN_LIST_MARKER) de una línea.
 * ────────────────────────────────────────────── */
static int list_marker_width(ParsedLine *line) {
    if (line->type != LINE_LIST_UNORDERED &&
        line->type != LINE_LIST_ORDERED)
        return 0;
    int width = 0;
    wchar_t wbuf[256];
    for (int i = 0; i < line->span_count; i++) {
        if (line->spans[i].type == SPAN_LIST_MARKER) {
            int wlen = utf8_to_wide(line->spans[i].text,
                                    (int)strlen(line->spans[i].text),
                                    wbuf, 256);
            for (int j = 0; j < wlen; j++) {
                int cw = wcwidth(wbuf[j]);
                width += (cw < 0) ? 1 : cw;
            }
        }
    }
    return width;
}

/* ──────────────────────────────────────────────
 * construir líneas visuales a partir de chars aplanados.
 * con wrap_words=1 parte en límites de palabra (espacio);
 * con wrap_words=0 parte por columna exacta.
 * cont_indent = sangría extra para líneas de continuación
 *               (usado en ítems de lista).
 * retorna cantidad de líneas visuales, o -1 en error.
 * ────────────────────────────────────────────── */
static int build_visual_lines(wchar_t *chars, int total, int avail_w,
                               int wrap_words, int cont_indent,
                               int **out_breaks) {
    if (total <= 0 || avail_w < 1) {
        *out_breaks = NULL;
        return (total <= 0) ? 0 : 1;
    }

    /* máximo posible: un char por línea */
    int *breaks = malloc(sizeof(int) * (size_t)(total + 1));
    if (!breaks) return -1;

    int pos = 0, line_count = 0;

    while (pos < total) {
        breaks[line_count++] = pos;

        /* primera línea usa todo el ancho; las siguientes descuentan sangría */
        int effective_w = (line_count == 1) ? avail_w
                                            : avail_w - cont_indent;
        if (effective_w < 1) effective_w = 1;

        int col = 0;
        int last_space_pos = -1;
        int line_end = pos;

        while (line_end < total) {
            int cw = wcwidth(chars[line_end]);
            if (cw < 0) cw = 1;
            if (col + cw > effective_w) break;
            if (wrap_words && chars[line_end] == L' ')
                last_space_pos = line_end;
            col += cw;
            line_end++;
        }

        /* word-wrap: si hay overflow a mitad de palabra, rebobinar
           al último espacio encontrado en esta línea */
        if (wrap_words && line_end < total && last_space_pos >= pos)
            line_end = last_space_pos;

        /* forzar avance si no cupo ni un carácter */
        if (line_end == pos) line_end = pos + 1;

        pos = line_end;

        /* saltar el espacio que disparó el wrap */
        if (wrap_words && pos < total && chars[pos] == L' ')
            pos++;
    }

    *out_breaks = breaks;
    return line_count;
}

/* ──────────────────────────────────────────────
 * cantidad de filas que ocupa la línea en pantalla
 * usa wcwidth para anchos reales (emoji=2, CJK=2)
 * ────────────────────────────────────────────── */
static int line_wrapped_rows(ParsedLine *line, int avail_w,
                              int wrap_words) {
    if (line->type == LINE_EMPTY) return 1;

    /* las filas de tabla de datos pueden ocupar múltiples
       líneas por wrapping de celdas */
    if (line->type == LINE_TABLE_HEADER ||
        line->type == LINE_TABLE_ROW) {

        int ncols = line->table_cols;
        if (ncols <= 0 || !line->table_widths) return 1;

        /* ── ajustar anchos (misma lógica que rendering) ── */
        int pipes_w   = ncols + 1;
        int cell_avail = avail_w - pipes_w;
        if (cell_avail < ncols * 3) cell_avail = ncols * 3;

        int total_req = 0;
        for (int c = 0; c < ncols; c++) total_req += line->table_widths[c];

        int *adj_w = malloc(sizeof(int) * (size_t)ncols);
        if (!adj_w) return 1;

        if (total_req > cell_avail) {
            int alloc = 0;
            for (int c = 0; c < ncols; c++) {
                adj_w[c] = line->table_widths[c] * cell_avail / total_req;
                if (adj_w[c] < 3) adj_w[c] = 3;
                alloc += adj_w[c];
            }
            int rem = cell_avail - alloc;
            for (int c = 0; c < ncols && rem > 0; c++) { adj_w[c]++; rem--; }
        } else {
            for (int c = 0; c < ncols; c++) adj_w[c] = line->table_widths[c];
        }

        /* ── encontrar rangos de spans por columna ── */
        int *cell_start = calloc((size_t)ncols, sizeof(int));
        int *cell_end   = calloc((size_t)ncols, sizeof(int));
        int ci = -1;
        for (int s = 0; s < line->span_count; s++) {
            if (line->spans[s].type == SPAN_TABLE_PIPE) {
                if (ci >= 0 && ci < ncols) cell_end[ci] = s;
                ci++;
                if (ci >= 0 && ci < ncols) cell_start[ci] = s + 1;
            }
        }
        if (ci >= 0 && ci < ncols) cell_end[ci] = line->span_count;

        /* ── calcular líneas por celda ── */
        int max_lines = 1;
        if (wrap_words) {
            /* word-wrap: aplanar cada celda y partir en límites de palabra */
            for (int c = 0; c < ncols; c++) {
                wchar_t *chars = NULL;
                chtype  *attrs = NULL;
                int total = flatten_span_range(line->spans,
                                                cell_start[c], cell_end[c],
                                                line->type, &chars, &attrs);
                if (total > 0) {
                    int *breaks = NULL;
                    int lines = build_visual_lines(chars, total, adj_w[c],
                                                    1, 0, &breaks);
                    if (lines > max_lines) max_lines = lines;
                    free(breaks);
                }
                free(chars);
                free(attrs);
            }
        } else {
            for (int c = 0; c < ncols; c++) {
                int cw = cell_spans_width(line->spans,
                                          cell_start[c], cell_end[c]);
                int lines = (cw > 0) ? ((cw + adj_w[c] - 1) / adj_w[c]) : 1;
                if (lines > max_lines) max_lines = lines;
            }
        }

        free(adj_w);
        free(cell_start);
        free(cell_end);
        return max_lines;
    }

    /* bordes de tabla y separadores siempre 1 línea */
    if (line->type == LINE_TABLE_SEP   ||
        line->type == LINE_TABLE_TOP   ||
        line->type == LINE_TABLE_HSEP  ||
        line->type == LINE_TABLE_RSEP  ||
        line->type == LINE_TABLE_BOTTOM)
        return 1;

    /* ── word-wrap: usar líneas visuales con corte en espacios ── */
    if (wrap_words) {
        wchar_t *chars = NULL;
        chtype  *attrs = NULL;
        int total = flatten_spans(line, &chars, &attrs);
        if (total > 0) {
            int *breaks = NULL;
            int rows = build_visual_lines(chars, total, avail_w, 1,
                                          list_marker_width(line), &breaks);
            free(chars);
            free(attrs);
            free(breaks);
            return rows > 0 ? rows : 1;
        }
        free(chars);
        free(attrs);
        /* si flatten falló, seguir con char-wrap clásico */
    }

    /* ── char-wrap clásico (comportamiento original) ── */
    int indent = list_marker_width(line);
    int col = 0, rows = 1;
    int eff_w = avail_w;
    wchar_t wbuf[8192];
    for (int i = 0; i < line->span_count; i++) {
        int wlen = utf8_to_wide(line->spans[i].text,
                                (int)strlen(line->spans[i].text),
                                wbuf, 8192);
        for (int j = 0; j < wlen; j++) {
            int cw = wcwidth(wbuf[j]);
            if (cw < 0) cw = 1;
            if (col + cw > eff_w) {
                col = indent;
                eff_w = avail_w - indent;
                rows++;
            }
            col += cw;
        }
    }
    return rows;
}

/* ──────────────────────────────────────────────
 * total de filas visuales de todo el documento
 * ────────────────────────────────────────────── */
static int total_visual_rows(Renderer *r) {
    int margin = r->show_numbers ? 6 : 2;
    int avail  = r->term_w - margin;
    if (avail < 1) avail = 1;
    int total = 0;
    for (int i = 0; i < r->doc->count; i++)
        total += line_wrapped_rows(&r->doc->lines[i], avail, r->wrap_words);
    return total;
}

/* ──────────────────────────────────────────────
 * renderizar una línea fuente en pantalla
 * retorna cuántas filas de pantalla se usaron
 * ────────────────────────────────────────────── */
static int render_source_line(Renderer *r, int line_idx,
                               int screen_y, int skip_rows) {
    if (screen_y >= r->content_h) return 0;

    ParsedLine *line = &r->doc->lines[line_idx];
    int margin  = r->show_numbers ? 6 : 2;
    int avail_w = r->term_w - margin;
    if (avail_w < 1) avail_w = 1;

    /* ── línea vacía ── */
    if (line->type == LINE_EMPTY) {
        if (skip_rows <= 0 && screen_y >= 0) {
            wmove(r->main_win, screen_y, 0);
            wclrtoeol(r->main_win);
        }
        return 1;
    }

    /* ── regla horizontal ── */
    if (line->type == LINE_HORIZONTAL_RULE) {
        if (skip_rows <= 0 && screen_y >= 0 && screen_y < r->content_h) {
            wmove(r->main_win, screen_y, margin);
            for (int x = 0; x < avail_w; x++)
                waddch(r->main_win, ACS_HLINE | COLOR_PAIR(CP_HR) | A_DIM);
        }
        return 1;
    }

    /* ── bordes de tabla (generados con anchos ajustados) ── */
    if (line->type == LINE_TABLE_TOP   ||
        line->type == LINE_TABLE_HSEP  ||
        line->type == LINE_TABLE_RSEP  ||
        line->type == LINE_TABLE_BOTTOM) {

        if (skip_rows > 0) return 1;
        if (screen_y < 0 || screen_y >= r->content_h) return 1;

        int ncols = line->table_cols;
        if (ncols <= 0 || !line->table_widths) return 1;

        /* calcular anchos ajustados (misma lógica que filas de datos) */
        int pipes_w = ncols + 1;
        int cell_avail = avail_w - pipes_w;
        if (cell_avail < ncols * 3) cell_avail = ncols * 3;

        int total_req = 0;
        for (int c = 0; c < ncols; c++) total_req += line->table_widths[c];

        int *adj_w = malloc(sizeof(int) * (size_t)ncols);
        if (!adj_w) return 1;

        if (total_req > cell_avail) {
            int alloc = 0;
            for (int c = 0; c < ncols; c++) {
                adj_w[c] = line->table_widths[c] * cell_avail / total_req;
                if (adj_w[c] < 3) adj_w[c] = 3;
                alloc += adj_w[c];
            }
            int rem = cell_avail - alloc;
            for (int c = 0; c < ncols && rem > 0; c++) { adj_w[c]++; rem--; }
        } else {
            for (int c = 0; c < ncols; c++) adj_w[c] = line->table_widths[c];
        }

        /* elegir caracteres según tipo de borde */
        const char *left, *mid, *right;
        if (line->type == LINE_TABLE_TOP) {
            left = "┌"; mid = "┬"; right = "┐";
        } else if (line->type == LINE_TABLE_BOTTOM) {
            left = "└"; mid = "┴"; right = "┘";
        } else {
            left = "├"; mid = "┼"; right = "┤";
        }

        /* convertir caracteres box-drawing a wide chars */
        wchar_t w_left[4], w_mid[4], w_right[4], w_h[4];
        int wl_l = utf8_to_wide(left,  (int)strlen(left),  w_left,  4);
        int wl_m = utf8_to_wide(mid,   (int)strlen(mid),   w_mid,   4);
        int wl_r = utf8_to_wide(right, (int)strlen(right), w_right, 4);
        int wl_h = utf8_to_wide("─",   3,                  w_h,     4);

        chtype attr = COLOR_PAIR(CP_TABLE_BORDER);
        int sx = margin;

        /* borde izquierdo */
        if (wl_l > 0 && sx < r->term_w) {
            wmove(r->main_win, screen_y, sx);
            wattron(r->main_win, attr);
            waddnwstr(r->main_win, w_left, 1);
            wattroff(r->main_win, attr);
            sx += wcwidth(w_left[0]);
        }

        for (int c = 0; c < ncols; c++) {
            /* relleno horizontal */
            for (int i = 0; i < adj_w[c] && sx < r->term_w; i++) {
                if (wl_h > 0) {
                    wmove(r->main_win, screen_y, sx);
                    wattron(r->main_win, attr);
                    waddnwstr(r->main_win, w_h, 1);
                    wattroff(r->main_win, attr);
                    sx += wcwidth(w_h[0]);
                }
            }
            /* conector (mid o right para el último) */
            if (c < ncols - 1 && wl_m > 0 && sx < r->term_w) {
                wmove(r->main_win, screen_y, sx);
                wattron(r->main_win, attr);
                waddnwstr(r->main_win, w_mid, 1);
                wattroff(r->main_win, attr);
                sx += wcwidth(w_mid[0]);
            }
        }

        /* borde derecho */
        if (wl_r > 0 && sx < r->term_w) {
            wmove(r->main_win, screen_y, sx);
            wattron(r->main_win, attr);
            waddnwstr(r->main_win, w_right, 1);
            wattroff(r->main_win, attr);
            sx += wcwidth(w_right[0]);
        }

        free(adj_w);

        /* limpiar resto */
        if (sx < r->term_w) {
            wmove(r->main_win, screen_y, sx);
            wclrtoeol(r->main_win);
        }
        return 1;
    }

    /* ── tabla (filas de datos y encabezados con wrapping) ── */
    if (line->type == LINE_TABLE_HEADER ||
        line->type == LINE_TABLE_ROW) {

        if (screen_y >= r->content_h) return 0;

        int ncols = line->table_cols;
        if (ncols <= 0 || !line->table_widths || !line->table_aligns)
            return 1;

        /* ── ajustar anchos al espacio disponible ── */
        int pipes_w   = ncols + 1;
        int cell_avail = avail_w - pipes_w;
        if (cell_avail < ncols * 3) cell_avail = ncols * 3;

        int total_req = 0;
        for (int c = 0; c < ncols; c++) total_req += line->table_widths[c];

        int *adj_w = malloc(sizeof(int) * (size_t)ncols);
        if (!adj_w) return 1;

        if (total_req > cell_avail) {
            int alloc = 0;
            for (int c = 0; c < ncols; c++) {
                adj_w[c] = line->table_widths[c] * cell_avail / total_req;
                if (adj_w[c] < 3) adj_w[c] = 3;
                alloc += adj_w[c];
            }
            int rem = cell_avail - alloc;
            for (int c = 0; c < ncols && rem > 0; c++) { adj_w[c]++; rem--; }
        } else {
            for (int c = 0; c < ncols; c++) adj_w[c] = line->table_widths[c];
        }

        /* ── encontrar rangos de spans por columna ── */
        int *cell_start = calloc((size_t)ncols, sizeof(int));
        int *cell_end   = calloc((size_t)ncols, sizeof(int));
        int ci = -1;
        for (int s = 0; s < line->span_count; s++) {
            if (line->spans[s].type == SPAN_TABLE_PIPE) {
                if (ci >= 0 && ci < ncols) cell_end[ci] = s;
                ci++;
                if (ci >= 0 && ci < ncols) cell_start[ci] = s + 1;
            }
        }
        if (ci >= 0 && ci < ncols) cell_end[ci] = line->span_count;

        /* ── preparar datos de celda para renderizado ── */
        int *cell_lines = malloc(sizeof(int) * (size_t)ncols);
        int  max_lines  = 1;

        /* arrays para word-wrap: solo se usan si r->wrap_words está activo */
        wchar_t **cell_chars  = NULL;
        chtype  **cell_attrs  = NULL;
        int     **cell_breaks = NULL;
        int      *cell_total  = NULL;

        if (r->wrap_words) {
            cell_chars  = calloc((size_t)ncols, sizeof(wchar_t*));
            cell_attrs  = calloc((size_t)ncols, sizeof(chtype*));
            cell_breaks = calloc((size_t)ncols, sizeof(int*));
            cell_total  = calloc((size_t)ncols, sizeof(int));

            for (int c = 0; c < ncols; c++) {
                if (cell_start[c] < cell_end[c]) {
                    cell_total[c] = flatten_span_range(
                        line->spans, cell_start[c], cell_end[c],
                        line->type, &cell_chars[c], &cell_attrs[c]);
                    if (cell_total[c] > 0) {
                        int *breaks = NULL;
                        cell_lines[c] = build_visual_lines(
                            cell_chars[c], cell_total[c], adj_w[c],
                            1, 0, &breaks);
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
        } else {
            int *cell_cw = malloc(sizeof(int) * (size_t)ncols);
            for (int c = 0; c < ncols; c++) {
                cell_cw[c] = cell_spans_width(line->spans,
                                              cell_start[c], cell_end[c]);
                cell_lines[c] = (cell_cw[c] > 0)
                    ? ((cell_cw[c] + adj_w[c] - 1) / adj_w[c])
                    : 1;
                if (cell_lines[c] > max_lines) max_lines = cell_lines[c];
            }
            free(cell_cw);
        }
        if (max_lines < 1) max_lines = 1;

        /* ── saltar sub-filas según skip_rows ── */
        int start_wr = (skip_rows > 0) ? skip_rows : 0;
        if (start_wr >= max_lines) {
            free(adj_w); free(cell_start); free(cell_end);
            free(cell_lines);
            if (r->wrap_words) {
                for (int c = 0; c < ncols; c++) {
                    free(cell_chars[c]); free(cell_attrs[c]);
                    free(cell_breaks[c]);
                }
                free(cell_chars); free(cell_attrs);
                free(cell_breaks); free(cell_total);
            }
            return max_lines;
        }

        /* ── dibujar cada sub-fila visual ── */
        int sy = screen_y;
        for (int wr = start_wr; wr < max_lines && sy < r->content_h; wr++) {

            int sx = margin;

            /* borde izquierdo */
            if (sx < r->term_w) {
                wmove(r->main_win, sy, sx);
                wattron(r->main_win, COLOR_PAIR(CP_TABLE_BORDER));
                waddch(r->main_win, ACS_VLINE);
                wattroff(r->main_win, COLOR_PAIR(CP_TABLE_BORDER));
                sx++;
            }

            for (int c = 0; c < ncols; c++) {
                int col_w = adj_w[c];

                if (wr < cell_lines[c] && cell_start[c] < cell_end[c]) {

                    if (r->wrap_words && cell_chars[c] && cell_breaks[c]) {
                        /* ── word-wrap: renderizar línea visual completa ── */
                        int line_start = cell_breaks[c][wr];
                        int line_end   = (wr + 1 < cell_lines[c])
                                         ? cell_breaks[c][wr + 1]
                                         : cell_total[c];

                        /* calcular ancho visual de esta línea */
                        int line_width = 0;
                        for (int k = line_start; k < line_end; k++) {
                            int cw = wcwidth(cell_chars[c][k]);
                            line_width += (cw < 0) ? 1 : cw;
                        }

                        /* alineación para celdas de una sola línea visual */
                        int align_pad = 0;
                        if (cell_lines[c] == 1 && line_width < col_w) {
                            int pad = col_w - line_width;
                            if (line->table_aligns[c] == 0)
                                align_pad = pad / 2;
                            else if (line->table_aligns[c] == 1)
                                align_pad = pad;
                            for (int p = 0; p < align_pad && sx < r->term_w; p++) {
                                wmove(r->main_win, sy, sx++);
                                waddch(r->main_win, ' ');
                            }
                        }

                        /* dibujar chars agrupando mismo atributo */
                        int ci = line_start;
                        while (ci < line_end && sx < r->term_w) {
                            int run_end = ci + 1;
                            while (run_end < line_end &&
                                   cell_attrs[c][run_end] == cell_attrs[c][ci])
                                run_end++;

                            wmove(r->main_win, sy, sx);
                            wattron(r->main_win, cell_attrs[c][ci]);
                            waddnwstr(r->main_win, cell_chars[c] + ci,
                                      run_end - ci);
                            wattroff(r->main_win, cell_attrs[c][ci]);

                            for (int k = ci; k < run_end; k++) {
                                int cw = wcwidth(cell_chars[c][k]);
                                sx += (cw < 0) ? 1 : cw;
                            }
                            ci = run_end;
                        }

                        /* rellenar espacio sobrante */
                        int fill = col_w - line_width - align_pad;
                        if (fill < 0) fill = 0;
                        for (int p = 0; p < fill && sx < r->term_w; p++) {
                            wmove(r->main_win, sy, sx++);
                            waddch(r->main_win, ' ');
                        }
                    } else {
                        /* ── char-wrap clásico ── */
                        int skip_cols = wr * col_w;
                        int render_w  = col_w;
                        int cell_cw = cell_spans_width(line->spans,
                                                       cell_start[c],
                                                       cell_end[c]);

                        if (wr == cell_lines[c] - 1) {
                            int remaining = cell_cw - skip_cols;
                            if (remaining < render_w) render_w = remaining;
                        }

                        int align_pad = 0;
                        if (cell_lines[c] == 1 && render_w < col_w) {
                            int pad = col_w - render_w;
                            if (line->table_aligns[c] == 0)
                                align_pad = pad / 2;
                            else if (line->table_aligns[c] == 1)
                                align_pad = pad;
                            for (int p = 0; p < align_pad && sx < r->term_w; p++) {
                                wmove(r->main_win, sy, sx++);
                                waddch(r->main_win, ' ');
                            }
                        }

                        int drawn = render_cell_slice(r->main_win, sy, sx,
                                                      line->spans,
                                                      cell_start[c], cell_end[c],
                                                      skip_cols, render_w,
                                                      line->type);
                        sx += drawn;

                        int fill = col_w - drawn - align_pad;
                        if (fill < 0) fill = 0;
                        for (int p = 0; p < fill && sx < r->term_w; p++) {
                            wmove(r->main_win, sy, sx++);
                            waddch(r->main_win, ' ');
                        }
                    }
                } else {
                    /* sub-fila sin contenido: espacios */
                    for (int p = 0; p < col_w && sx < r->term_w; p++) {
                        wmove(r->main_win, sy, sx++);
                        waddch(r->main_win, ' ');
                    }
                }

                /* separador entre columnas */
                if (c < ncols - 1) {
                    if (sx < r->term_w) {
                        wmove(r->main_win, sy, sx);
                        wattron(r->main_win, COLOR_PAIR(CP_TABLE_BORDER));
                        waddch(r->main_win, ACS_VLINE);
                        wattroff(r->main_win, COLOR_PAIR(CP_TABLE_BORDER));
                        sx++;
                    }
                }
            }

            /* borde derecho */
            if (sx < r->term_w) {
                wmove(r->main_win, sy, sx);
                wattron(r->main_win, COLOR_PAIR(CP_TABLE_BORDER));
                waddch(r->main_win, ACS_VLINE);
                wattroff(r->main_win, COLOR_PAIR(CP_TABLE_BORDER));
                sx++;
            }

            /* limpiar resto de la línea */
            if (sx < r->term_w) {
                wmove(r->main_win, sy, sx);
                wclrtoeol(r->main_win);
            }

            sy++;
        }

        free(adj_w); free(cell_start); free(cell_end);
        free(cell_lines);
        if (r->wrap_words) {
            for (int c = 0; c < ncols; c++) {
                free(cell_chars[c]);
                free(cell_attrs[c]);
                free(cell_breaks[c]);
            }
            free(cell_chars); free(cell_attrs);
            free(cell_breaks); free(cell_total);
        }

        int used = sy - screen_y;
        if (used < 1) used = 1;
        return used;
    }

    /* ── tabla: separador (por compatibilidad, no usado actualmente) ── */
    if (line->type == LINE_TABLE_SEP) {
        if (skip_rows > 0) return 1;
        if (screen_y < 0 || screen_y >= r->content_h) return 1;
        wmove(r->main_win, screen_y, margin);
        for (int x = 0; x < avail_w && x + margin < r->term_w; x++)
            waddch(r->main_win, ACS_HLINE | COLOR_PAIR(CP_TABLE_BORDER));
        if (margin + avail_w < r->term_w) {
            wmove(r->main_win, screen_y, margin + avail_w);
            wclrtoeol(r->main_win);
        }
        return 1;
    }

    /* ── word-wrap: aplanar spans y partir en límites de palabra ── */
    if (r->wrap_words) {
        wchar_t *chars = NULL;
        chtype  *attrs = NULL;
        int total = flatten_spans(line, &chars, &attrs);

        if (total > 0) {
            int list_ind = list_marker_width(line);
            int *breaks = NULL;
            int nlines = build_visual_lines(chars, total, avail_w, 1,
                                            list_ind, &breaks);

            int used = 0;
            for (int vl = 0; vl < nlines; vl++) {
                int start = breaks[vl];
                int end   = (vl + 1 < nlines) ? breaks[vl + 1] : total;

                int row = screen_y + vl - skip_rows;
                if (vl >= skip_rows && row >= 0 && row < r->content_h) {
                    /* líneas de continuación de lista llevan sangría */
                    int sx = margin + ((vl > 0) ? list_ind : 0);
                    int ci = start;
                    while (ci < end) {
                        /* agrupar chars consecutivos con mismo atributo */
                        int run_end = ci + 1;
                        while (run_end < end && attrs[run_end] == attrs[ci])
                            run_end++;

                        wmove(r->main_win, row, sx);
                        wattron(r->main_win, attrs[ci]);
                        waddnwstr(r->main_win, chars + ci, run_end - ci);
                        wattroff(r->main_win, attrs[ci]);

                        /* avanzar sx según ancho visual del grupo */
                        for (int k = ci; k < run_end; k++) {
                            int cw = wcwidth(chars[k]);
                            sx += (cw < 0) ? 1 : cw;
                        }
                        ci = run_end;
                    }
                }
                used = vl + 1 - skip_rows;
            }

            free(chars);
            free(attrs);
            free(breaks);

            if (used < 0) used = 0;
            if (screen_y + used > r->content_h)
                used = r->content_h - screen_y;
            return used;
        }

        free(chars);
        free(attrs);
        /* si flatten falló, continuar con char-wrap clásico */
    }

    /* ── char-wrap clásico: caminar spans como wide chars ── */
    int list_ind = list_marker_width(line);
    int col      = 0;
    int wrap_row = 0;
    int used     = 0;
    wchar_t wbuf[16384];

    for (int s = 0; s < line->span_count; s++) {
        Span  *span = &line->spans[s];
        chtype attr = span_attr(span->type, line->type);
        int wlen = utf8_to_wide(span->text, (int)strlen(span->text),
                                wbuf, 16384);
        int wi = 0;

        while (wi < wlen) {
            int eff_w = (wrap_row == 0) ? avail_w
                                        : avail_w - list_ind;
            if (eff_w < 1) eff_w = 1;
            int avail_cols = eff_w - col;
            if (avail_cols <= 0) {
                col = list_ind;
                wrap_row++;
                eff_w = avail_w - list_ind;
                if (eff_w < 1) eff_w = 1;
                avail_cols = eff_w - list_ind;
                if (avail_cols < 0) avail_cols = 0;
            }

            /* contar cuántos wide chars caben por ancho real */
            int seg_chars = 0;
            int seg_width = 0;
            int j = wi;
            while (j < wlen && seg_width < avail_cols) {
                int cw = wcwidth(wbuf[j]);
                if (cw < 0) cw = 1;
                if (seg_width + cw > avail_cols) break;
                seg_width += cw;
                seg_chars++;
                j++;
            }
            if (seg_chars == 0 && j < wlen) {
                seg_chars = 1;
                seg_width = avail_cols;
            }

            /* ── imprimir con waddnwstr ── */
            int row = screen_y + wrap_row - skip_rows;
            if (wrap_row >= skip_rows &&
                row >= 0 && row < r->content_h && seg_chars > 0) {
                wmove(r->main_win, row, margin + col);
                wattron(r->main_win, attr);
                waddnwstr(r->main_win, wbuf + wi, seg_chars);
                wattroff(r->main_win, attr);
            }

            wi  += seg_chars;
            col += seg_width;

            if (col >= eff_w) {
                col = list_ind;
                wrap_row++;
            }
        }
    }

    used = (wrap_row + 1) - skip_rows;
    if (screen_y + used > r->content_h)
        used = r->content_h - screen_y;
    return used;
}

/* ──────────────────────────────────────────────
 * dibujar barra de estado
 * ────────────────────────────────────────────── */
static void draw_status(Renderer *r) {
    werase(r->status_win);
    wbkgd(r->status_win, COLOR_PAIR(CP_STATUSBAR));

    /* calcular porcentaje visual */
    int total = total_visual_rows(r);
    int cur   = 0;
    int avail_w = r->term_w - (r->show_numbers ? 6 : 2);
    if (avail_w < 1) avail_w = 1;
    for (int i = 0; i < r->scroll_line && i < r->doc->count; i++)
        cur += line_wrapped_rows(&r->doc->lines[i], avail_w, r->wrap_words);
    cur += r->scroll_skip;
    if (cur < 0) cur = 0;

    int pct = total > 0 ? (cur * 100 + total / 2) / total : 0;

    /* construir ruta acortada */
    const char *fname = r->filename ? r->filename : "(stdin)";
    const char *slash = strrchr(fname, '/');
    const char *show  = slash ? slash + 1 : fname;

    mvwprintw(r->status_win, 0, 0,
              " visormd  %s  L%d/%d  %d%%  [q]uit [arrows/jk] "
              "[PgUp/PgDn] [g/G] [n]ums [w]rap [F2]tema",
              show,
              r->scroll_line + 1, r->doc->count,
              pct);
    wrefresh(r->status_win);
}

/* ──────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────── */

Renderer *renderer_create(Document *doc, const char *filename) {
    Renderer *r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;

    r->doc          = doc;
    r->filename     = strdup(filename ? filename : "stdin");
    r->show_numbers = 0;
    r->wrap_words   = 1;
    r->theme_idx    = 0;
    r->scroll_line  = 0;
    r->scroll_skip  = 0;
    r->anchor_line  = 0;
    r->anchor_skip  = 0;

    /* iniciar ncurses */
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        r->theme_idx = config_load_theme();
        init_colors(r->theme_idx);
    }

    refresh();

    r->term_w = getmaxx(stdscr);
    r->term_h = getmaxy(stdscr);
    r->content_h = r->term_h - 1;
    if (r->content_h < 1) r->content_h = 1;

    r->main_win   = newwin(r->content_h, r->term_w, 0, 0);
    r->status_win = newwin(1, r->term_w, r->content_h, 0);
    keypad(r->main_win, TRUE);
    scrollok(r->main_win, FALSE);

    return r;
}

void renderer_free(Renderer *r) {
    if (!r) return;
    free(r->filename);
    delwin(r->main_win);
    delwin(r->status_win);
    endwin();
    free(r);
}

void renderer_resize(Renderer *r) {
    endwin();
    refresh();

    r->term_w = getmaxx(stdscr);
    r->term_h = getmaxy(stdscr);
    r->content_h = r->term_h - 1;
    if (r->content_h < 1) r->content_h = 1;

    wresize(r->main_win,   r->content_h, r->term_w);
    wresize(r->status_win, 1,            r->term_w);
    mvwin(r->status_win,   r->content_h, 0);

    /* reclampar scroll después de resize */
    int margin = r->show_numbers ? 6 : 2;
    int avail  = r->term_w - margin;
    if (avail < 1) avail = 1;

    /* ajustar scroll_skip a las nuevas dimensiones */
    while (r->scroll_line < r->doc->count) {
        int rows = line_wrapped_rows(&r->doc->lines[r->scroll_line], avail,
                                      r->wrap_words);
        if (r->scroll_skip < rows) break;
        r->scroll_skip -= rows;
        r->scroll_line++;
    }
    if (r->scroll_line >= r->doc->count) {
        r->scroll_line = r->doc->count > 0 ? r->doc->count - 1 : 0;
        r->scroll_skip = 0;
    }
}

/* ──────────────────────────────────────────────
 * dibujar el documento completo
 * ────────────────────────────────────────────── */
void renderer_draw(Renderer *r) {
    werase(r->main_win);

    int sy  = 0;
    int sl  = r->scroll_line;
    int skip = r->scroll_skip;

    for (int i = sl; i < r->doc->count && sy < r->content_h; i++) {
        int used = render_source_line(r, i, sy, skip);
        sy  += used;
        skip = 0;  /* solo la primera línea tiene skip */
    }

    /* limpiar filas sobrantes */
    for (; sy < r->content_h; sy++) {
        wmove(r->main_win, sy, 0);
        wclrtoeol(r->main_win);
    }

    draw_status(r);
    wrefresh(r->main_win);
}

/* ──────────────────────────────────────────────
 * navegación y manejo de input
 * ────────────────────────────────────────────── */

static void theme_selector_show(Renderer *r);

static int avail_width(Renderer *r) {
    int m = r->show_numbers ? 6 : 2;
    int w = r->term_w - m;
    return w < 1 ? 1 : w;
}

/* avanzar N filas visuales */
static void scroll_down(Renderer *r, int rows) {
    int aw = avail_width(r);
    while (rows > 0 && r->scroll_line < r->doc->count) {
        int lr = line_wrapped_rows(&r->doc->lines[r->scroll_line], aw,
                                   r->wrap_words);
        int left = lr - r->scroll_skip;
        if (rows < left) {
            r->scroll_skip += rows;
            return;
        }
        rows -= left;
        r->scroll_line++;
        r->scroll_skip = 0;
    }
    /* no pasarse del final */
    if (r->scroll_line >= r->doc->count) {
        r->scroll_line = r->doc->count > 0 ? r->doc->count - 1 : 0;
        r->scroll_skip = 0;
    }
}

/* retroceder N filas visuales */
static void scroll_up(Renderer *r, int rows) {
    int aw = avail_width(r);
    while (rows > 0) {
        if (rows <= r->scroll_skip) {
            r->scroll_skip -= rows;
            return;
        }
        rows -= r->scroll_skip;
        r->scroll_skip = 0;
        if (r->scroll_line <= 0) return;
        r->scroll_line--;
        r->scroll_skip = line_wrapped_rows(&r->doc->lines[r->scroll_line], aw,
                                            r->wrap_words);
        if (r->scroll_skip > 0) r->scroll_skip--;
        if (rows > 0) rows--; /* ya retrocedimos una fila */
    }
}

int renderer_handle_input(Renderer *r, int ch) {
    switch (ch) {
    case 'q':
    case 'Q':
        return 1;

    case 'j':
    case KEY_DOWN:
        scroll_down(r, 1);
        break;

    case 'k':
    case KEY_UP:
        scroll_up(r, 1);
        break;

    case ' ':
    case KEY_NPAGE:  /* Page Down */
        scroll_down(r, r->content_h);
        break;

    case 'b':
    case KEY_PPAGE:  /* Page Up */
        scroll_up(r, r->content_h);
        break;

    case 'g':
        /* ir al principio */
        r->scroll_line = 0;
        r->scroll_skip = 0;
        break;

    case 'G':
        /* ir al final (mostrar última pantalla) */
        if (r->doc->count > 0) {
            r->scroll_line = r->doc->count - 1;
            r->scroll_skip = 0;
            /* desplazar hacia atrás hasta llenar la pantalla */
            for (int i = 0; i < r->content_h - 1; i++)
                scroll_up(r, 1);
        }
        break;

    case 'n':
    case 'N':
        r->show_numbers = !r->show_numbers;
        break;

    case 'w':
    case 'W':
        r->wrap_words = !r->wrap_words;
        break;

    case KEY_HOME:
        r->scroll_line = 0;
        r->scroll_skip = 0;
        break;

    case KEY_END:
        if (r->doc->count > 0) {
            r->scroll_line = r->doc->count - 1;
            r->scroll_skip = 0;
            for (int i = 0; i < r->content_h - 1; i++)
                scroll_up(r, 1);
        }
        break;

    case KEY_RESIZE:
        renderer_resize(r);
        break;

    case KEY_F(2):
        theme_selector_show(r);
        break;

    default:
        return 0;
    }

    /* reclampar después de cualquier movimiento */
    if (r->scroll_line < 0) {
        r->scroll_line = 0;
        r->scroll_skip = 0;
    }
    if (r->scroll_line >= r->doc->count) {
        r->scroll_line = r->doc->count > 0 ? r->doc->count - 1 : 0;
        r->scroll_skip = 0;
    }

    return 0;
}

/* ──────────────────────────────────────────────
 * selector de temas (overlay tipo htop F2)
 * ────────────────────────────────────────────── */
static void theme_selector_show(Renderer *r) {
    int nthemes = theme_get_count();
    int box_w   = 40;
    int box_h   = nthemes + 4;  /* borde + título + items + ayuda + borde */
    int start_x = (r->term_w - box_w) / 2;
    int start_y = (r->content_h - box_h) / 2;
    if (start_y < 0) start_y = 0;
    if (box_w > r->term_w) box_w = r->term_w;
    if (box_h > r->content_h) box_h = r->content_h;

    WINDOW *overlay = newwin(box_h, box_w, start_y, start_x);
    keypad(overlay, TRUE);
    curs_set(0);

    int selected = r->theme_idx;
    int done     = 0;

    while (!done) {
        werase(overlay);
        box(overlay, 0, 0);

        /* título */
        mvwprintw(overlay, 1, (box_w - 17) / 2, " Seleccionar Tema ");

        /* listar temas */
        int visible = box_h - 4;  /* filas disponibles para items */
        for (int i = 0; i < nthemes && i < visible; i++) {
            int y = i + 2;
            if (i == selected)
                wattron(overlay, A_REVERSE);

            mvwprintw(overlay, y, 2, " %c %-30s",
                      (i == r->theme_idx) ? '*' : ' ',
                      theme_get_by_index(i)->name);

            if (i == selected)
                wattroff(overlay, A_REVERSE);
        }

        /* ayuda */
        mvwprintw(overlay, box_h - 2, 2,
                  "Enter=seleccionar  Esc=cancelar");

        wrefresh(overlay);

        int ch = wgetch(overlay);
        switch (ch) {
        case KEY_UP:
        case 'k':
            if (selected > 0) selected--;
            break;
        case KEY_DOWN:
        case 'j':
            if (selected < nthemes - 1) selected++;
            break;
        case '\n':
        case KEY_ENTER:
        case '\r':
            r->theme_idx = selected;
            init_colors(selected);
            config_save_theme(theme_get_by_index(selected)->id);
            done = 1;
            break;
        case 27:   /* Esc */
        case 'q':
            done = 1;
            break;
        case KEY_RESIZE:
            renderer_resize(r);
            delwin(overlay);
            box_w = 40;
            box_h = nthemes + 4;
            start_x = (r->term_w - box_w) / 2;
            start_y = (r->content_h - box_h) / 2;
            if (start_y < 0) start_y = 0;
            if (box_w > r->term_w) box_w = r->term_w;
            if (box_h > r->content_h) box_h = r->content_h;
            overlay = newwin(box_h, box_w, start_y, start_x);
            keypad(overlay, TRUE);
            break;
        }
    }

    delwin(overlay);
    curs_set(0);
    renderer_draw(r);
}

int renderer_getch(Renderer *r) {
    (void)r;
    return wgetch(stdscr);
}
