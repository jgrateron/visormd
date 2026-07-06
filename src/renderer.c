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

    /* SPAN_NORMAL hereda el color del tipo de línea */
    if (st == SPAN_NORMAL || st == SPAN_LIST_MARKER) {
        if (st == SPAN_LIST_MARKER) { cp = CP_LIST_MARKER; }
        else {
            switch (lt) {
            case LINE_HEADING_1: cp = CP_HEADING_1; at = A_BOLD;   break;
            case LINE_HEADING_2: cp = CP_HEADING_2; at = A_BOLD;   break;
            case LINE_HEADING_3: cp = CP_HEADING_3; at = A_BOLD;   break;
            case LINE_HEADING_4: cp = CP_HEADING_4;                break;
            case LINE_HEADING_5: cp = CP_HEADING_5;                break;
            case LINE_HEADING_6: cp = CP_HEADING_6;                break;
            case LINE_CODE_BLOCK:cp = CP_CODE_BLOCK;               break;
            case LINE_BLOCKQUOTE:cp = CP_BLOCKQUOTE;               break;
            default:             cp = CP_DEFAULT;                  break;
            }
        }
    } else {
        switch (st) {
        case SPAN_BOLD:        cp = CP_BOLD;   at = A_BOLD;        break;
        case SPAN_ITALIC:      cp = CP_ITALIC; at = A_ITALIC;      break;
        case SPAN_BOLD_ITALIC: cp = CP_BOLD;   at = A_BOLD|A_ITALIC;break;
        case SPAN_CODE:        cp = CP_CODE;                       break;
        case SPAN_LINK_TEXT:   cp = CP_LINK;   at = A_UNDERLINE;   break;
        case SPAN_LINK_URL:    cp = CP_LINK;                       break;
        default:               cp = CP_DEFAULT;                    break;
        }
    }
    return COLOR_PAIR(cp) | at;
}

/* ──────────────────────────────────────────────
 * cantidad de filas que ocupa la línea en pantalla
 * usa wcwidth para anchos reales (emoji=2, CJK=2)
 * ────────────────────────────────────────────── */
static int line_wrapped_rows(ParsedLine *line, int avail_w) {
    if (line->type == LINE_EMPTY) return 1;
    int col = 0, rows = 1;
    wchar_t wbuf[8192];
    for (int i = 0; i < line->span_count; i++) {
        int wlen = utf8_to_wide(line->spans[i].text,
                                (int)strlen(line->spans[i].text),
                                wbuf, 8192);
        for (int j = 0; j < wlen; j++) {
            int cw = wcwidth(wbuf[j]);
            if (cw < 0) cw = 1;
            if (col + cw > avail_w) { col = 0; rows++; }
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
        total += line_wrapped_rows(&r->doc->lines[i], avail);
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

    /* ── caminar spans como wide chars ── */
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
            int avail_cols = avail_w - col;
            if (avail_cols <= 0) {
                col = 0;
                wrap_row++;
                avail_cols = avail_w;
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

            if (col >= avail_w) {
                col = 0;
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
        cur += line_wrapped_rows(&r->doc->lines[i], avail_w);
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
    r->wrap_words   = 0;
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
        int rows = line_wrapped_rows(&r->doc->lines[r->scroll_line], avail);
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
        int lr = line_wrapped_rows(&r->doc->lines[r->scroll_line], aw);
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
        r->scroll_skip = line_wrapped_rows(&r->doc->lines[r->scroll_line], aw);
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
