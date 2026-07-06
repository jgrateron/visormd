#ifndef CAT_RENDERER_H
#define CAT_RENDERER_H

#include "parser.h"

/* Renderiza el documento a stdout con códigos ANSI y termina.
   Similar a como funciona cat, pero con resaltado de sintaxis
   Markdown aplicado. */
void cat_render(Document *doc);

#endif
