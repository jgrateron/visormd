#ifndef RENDERER_H
#define RENDERER_H

#include "parser.h"

/* ── estado del renderer (ncurses + scroll) ── */
typedef struct Renderer Renderer;

Renderer *renderer_create(Document *doc, const char *filename);
void      renderer_free(Renderer *r);
void      renderer_resize(Renderer *r);
void      renderer_draw(Renderer *r);

/* retorna 0 para seguir, 1 para salir */
int       renderer_handle_input(Renderer *r, int ch);

/* espera una tecla y la retorna */
int       renderer_getch(Renderer *r);

#endif
