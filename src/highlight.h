#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "parser.h"

/* ── estado del resaltador (persiste entre líneas) ── */
typedef struct {
    int in_block_comment;   /* dentro de un comentario / * ... * / */
    int in_triple_quote;    /* dentro de un string triple: 1="""  2=''' */
    int in_xml_comment;     /* dentro de un comentario XML <!-- ... --> */
} HighlightState;

/* ── inicializar el estado ── */
void highlight_state_init(HighlightState *st);

/* ── verificar si un lenguaje tiene resaltado ── */
int  highlight_supported(const char *lang);

/* ── tokenizar una línea de código fuente en spans ──
 *   line  : ParsedLine a poblar (se le añaden spans)
 *   text  : texto fuente crudo de una línea
 *   lang  : identificador del lenguaje ("c","cpp","java","javascript","js")
 *   state : estado que persiste entre llamadas (para comentarios multilínea)
 *   retorna 0 en éxito, -1 en error                       ── */
int  highlight_line(ParsedLine *line, const char *text,
                    const char *lang, HighlightState *state);

#endif
