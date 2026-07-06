#include "buffer.h"
#include "parser.h"
#include "renderer.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [OPCIONES] <archivo.md>\n"
            "\n"
            "Visor interactivo de Markdown para terminal.\n"
            "\n"
            "Teclas durante la visualización:\n"
            "  q         Salir\n"
            "  j / ↓     Desplazar una línea hacia abajo\n"
            "  k / ↑     Desplazar una línea hacia arriba\n"
            "  Espacio   Avanzar una página\n"
            "  b         Retroceder una página\n"
            "  g / Home  Ir al principio del documento\n"
            "  G / End   Ir al final del documento\n"
            "  n         Alternar números de línea\n"
            "  w         Alternar ajuste de palabras\n"
            "  F2        Seleccionar tema de colores\n"
            "\n"
            "Ejemplo: %s README.md\n",
            prog, prog);
}

int main(int argc, char **argv) {
    const char *filename = NULL;

    /* parsear argumentos */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "%s: opción desconocida: %s\n", argv[0], argv[i]);
            usage(argv[0]);
            return 1;
        }
        filename = argv[i];
    }

    if (!filename) {
        usage(argv[0]);
        return 1;
    }

    /* ── configurar locale UTF-8 (prueba varios) ── */
    char *loc = setlocale(LC_ALL, "");
    if (!loc || !strstr(loc, "UTF")) {
        if (!setlocale(LC_ALL, "C.UTF-8"))
            setlocale(LC_ALL, "en_US.UTF-8");
    }

    /* ── leer archivo ── */
    TextBuffer *buf = buffer_create();
    if (!buf) {
        fprintf(stderr, "%s: error de memoria\n", argv[0]);
        return 1;
    }

    if (buffer_load_file(buf, filename) != 0) {
        fprintf(stderr, "%s: no se pudo leer '%s'\n", argv[0], filename);
        buffer_free(buf);
        return 1;
    }

    /* ── parsear markdown ── */
    Document *doc = doc_create();
    if (!doc) {
        fprintf(stderr, "%s: error de memoria\n", argv[0]);
        buffer_free(buf);
        return 1;
    }
    doc_parse(doc, buf->lines, buf->count);
    buffer_free(buf);  /* ya no necesitamos las líneas crudas */

    /* ── iniciar renderer ncurses ── */
    Renderer *renderer = renderer_create(doc, filename);
    if (!renderer) {
        fprintf(stderr, "%s: error al iniciar ncurses\n", argv[0]);
        doc_free(doc);
        return 1;
    }

    /* ── bucle principal ── */
    renderer_draw(renderer);
    int quit = 0;
    while (!quit) {
        int ch = renderer_getch(renderer);
        quit = renderer_handle_input(renderer, ch);
        if (!quit) renderer_draw(renderer);
    }

    /* ── limpiar ── */
    renderer_free(renderer);
    doc_free(doc);

    return 0;
}
