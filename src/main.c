#include "buffer.h"
#include "cat_renderer.h"
#include "parser.h"
#include "renderer.h"
#include <dirent.h>
#include <glob.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [OPCIONES] [archivo.md | directorio | glob ...]\n"
            "\n"
            "Visor interactivo de Markdown para terminal.\n"
            "Acepta múltiples archivos, directorios (busca *.md) y patrones glob.\n"
            "\n"
            "Opciones:\n"
            "  -c, --cat   Volcar el contenido renderizado a stdout y salir\n"
            "  -h, --help  Mostrar esta ayuda\n"
            "\n"
            "Teclas durante la visualización interactiva:\n"
            "  q         Salir\n"
            "  r         Recargar el archivo desde disco\n"
            "  j / ↓     Desplazar una línea hacia abajo\n"
            "  k / ↑     Desplazar una línea hacia arriba\n"
            "  Espacio   Avanzar una página\n"
            "  b         Retroceder una página\n"
            "  g / Home  Ir al principio del documento\n"
            "  G / End   Ir al final del documento\n"
            "  n         Alternar números de línea\n"
            "  w         Alternar ajuste de palabras\n"
            "  F1        Acerca de VisorMD\n"
            "  F2        Seleccionar tema de colores\n"
            "  F4        Alternar vista renderizada / texto crudo\n"
            "\n"
            "Ejemplos:\n"
            "  %s README.md               # un solo archivo\n"
            "  %s -c README.md            # volcar a consola\n"
            "  %s docs/                   # todos los *.md en el directorio\n"
            "  %s *.md                    # expansión shell de glob\n"
            "  %s README.md CHANGES.md    # múltiples archivos\n"
            "  cat README.md | %s         # pipe → interactivo\n"
            "  cat README.md | %s -c      # pipe → volcar a consola\n",
            prog, prog, prog, prog, prog, prog, prog, prog);
}

/* ──────────────────────────────────────────────
 * comparador para ordenar strings (qsort)
 * ────────────────────────────────────────────── */
static int cmpstring(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* ──────────────────────────────────────────────
 * expandir un argumento en una lista de archivos
 * - si es directorio → buscar *.md dentro
 * - si contiene metacaracteres glob → glob()
 * - si es archivo regular → usarlo tal cual
 * retorna NULL si no se encontró nada
 * ────────────────────────────────────────────── */
static char **expand_arg(const char *arg, int *out_count) {
    struct stat st;
    char **files = NULL;
    *out_count = 0;

    /* ¿es un directorio? */
    if (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* escanear en busca de *.md */
        DIR *dir = opendir(arg);
        if (!dir) return NULL;

        /* evitar doble slash: quitar / final si existe */
        size_t arg_len = strlen(arg);
        int has_slash = (arg_len > 0 && arg[arg_len - 1] == '/');

        int cap = 0, cnt = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            size_t nl = strlen(name);
            /* solo archivos .md (ignorar ocultos) */
            if (nl < 3) continue;
            if (strcmp(name + nl - 3, ".md") != 0) continue;
            if (name[0] == '.') continue;

            if (cnt >= cap) {
                cap = cap ? cap * 2 : 16;
                files = realloc(files, sizeof(char *) * (size_t)(cap + 1));
            }
            size_t pl = arg_len + 1 + nl + 1;
            char *full = malloc(pl);
            if (has_slash)
                snprintf(full, pl, "%s%s", arg, name);
            else
                snprintf(full, pl, "%s/%s", arg, name);
            files[cnt++] = full;
        }
        closedir(dir);

        if (cnt > 0) {
            qsort(files, (size_t)cnt, sizeof(char *), cmpstring);
            files[cnt] = NULL;
            *out_count = cnt;
            return files;
        }
        free(files);
        return NULL;
    }

    /* ¿contiene metacaracteres glob? (* ? [ ) */
    int has_glob = 0;
    for (const char *p = arg; *p; p++) {
        if (*p == '*' || *p == '?' || *p == '[') { has_glob = 1; break; }
    }

    if (has_glob) {
        glob_t gr = {0};
        if (glob(arg, 0, NULL, &gr) == 0 && gr.gl_pathc > 0) {
            files = malloc(sizeof(char *) * (gr.gl_pathc + 1));
            for (size_t i = 0; i < gr.gl_pathc; i++)
                files[i] = strdup(gr.gl_pathv[i]);
            files[gr.gl_pathc] = NULL;
            qsort(files, gr.gl_pathc, sizeof(char *), cmpstring);
            *out_count = (int)gr.gl_pathc;
            globfree(&gr);
            return files;
        }
        globfree(&gr);
        return NULL;
    }

    /* archivo regular (o lo que sea, se intenta abrir después) */
    files = malloc(sizeof(char *) * 2);
    files[0] = strdup(arg);
    files[1] = NULL;
    *out_count = 1;
    return files;
}

/* ──────────────────────────────────────────────
 * insertar separador entre archivos
 * ────────────────────────────────────────────── */
static void add_separator(TextBuffer *buf, const char *filepath) {
    char sep[512];
    snprintf(sep, sizeof(sep),
             "## ====================== %s ======================", filepath);
    buffer_add_line(buf, "");
    buffer_add_line(buf, sep);
    buffer_add_line(buf, "");
}

/* ──────────────────────────────────────────────
 * cargar un archivo en el buffer
 * ────────────────────────────────────────────── */
static int append_file(TextBuffer *buf, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return -1;

    char line_buf[65536];
    while (fgets(line_buf, sizeof(line_buf), fp)) {
        size_t len = strlen(line_buf);
        while (len > 0 &&
               (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r'))
            line_buf[--len] = '\0';
        buffer_add_line(buf, line_buf);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    int cat_mode = 0;

    /* ── recolectar argumentos de archivo ── */
    char **file_args = malloc(sizeof(char *) * (size_t)(argc + 1));
    int    file_count = 0;
    if (!file_args) return 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            free(file_args);
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cat") == 0) {
            cat_mode = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "%s: opción desconocida: %s\n", argv[0], argv[i]);
            usage(argv[0]);
            free(file_args);
            return 1;
        }
        file_args[file_count++] = argv[i];
    }

    /* ── sin archivos ni stdin → mostrar ayuda ── */
    if (file_count == 0 && isatty(STDIN_FILENO)) {
        usage(argv[0]);
        free(file_args);
        return 1;
    }

    /* ── configurar locale UTF-8 ── */
    char *loc = setlocale(LC_ALL, "");
    if (!loc || !strstr(loc, "UTF")) {
        if (!setlocale(LC_ALL, "C.UTF-8"))
            setlocale(LC_ALL, "en_US.UTF-8");
    }

    /* ── expandir argumentos en lista final de archivos ── */
    char **all_files = NULL;
    int    all_count = 0;
    int    all_cap   = 0;

    for (int i = 0; i < file_count; i++) {
        int   n = 0;
        char **expanded = expand_arg(file_args[i], &n);
        if (!expanded) {
            fprintf(stderr, "%s: no se encontraron archivos para '%s'\n",
                    argv[0], file_args[i]);
            continue;
        }
        for (int j = 0; j < n; j++) {
            if (all_count >= all_cap) {
                all_cap = all_cap ? all_cap * 2 : 32;
                all_files = realloc(all_files,
                                    sizeof(char *) * (size_t)(all_cap + 1));
            }
            all_files[all_count++] = expanded[j];
        }
        free(expanded); /* solo el array de punteros, los strings se quedan */
    }
    free(file_args);

    /* ── crear buffer y cargar archivos ── */
    TextBuffer *buf = buffer_create();
    if (!buf) {
        fprintf(stderr, "%s: error de memoria\n", argv[0]);
        for (int i = 0; i < all_count; i++) free(all_files[i]);
        free(all_files);
        return 1;
    }

    const char *first_file = NULL;

    if (all_count > 0) {
        /* cargar múltiples archivos con separadores */
        for (int i = 0; i < all_count; i++) {
            if (!first_file) first_file = all_files[i];

            if (all_count > 1)
                add_separator(buf, all_files[i]);

            if (append_file(buf, all_files[i]) != 0) {
                fprintf(stderr, "%s: no se pudo leer '%s'\n",
                        argv[0], all_files[i]);
            }
        }
    } else {
        /* leer desde stdin */
        if (buffer_load_stdin(buf) != 0) {
            fprintf(stderr, "%s: error al leer stdin\n", argv[0]);
            buffer_free(buf);
            for (int i = 0; i < all_count; i++) free(all_files[i]);
            free(all_files);
            return 1;
        }
    }

    /* liberar lista de archivos */
    for (int i = 0; i < all_count; i++) free(all_files[i]);
    free(all_files);

    if (buf->count == 0) {
        fprintf(stderr, "%s: nada que mostrar\n", argv[0]);
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

    if (cat_mode) {
        /* ── modo cat: volcar a stdout y salir ── */
        cat_render(doc);
        doc_free(doc);
    } else {
        /* ── si leímos de pipe, reconectar stdin a /dev/tty ── */
        if (!isatty(STDIN_FILENO)) {
            if (!freopen("/dev/tty", "r", stdin)) {
                fprintf(stderr,
                        "%s: no se puede acceder al terminal para modo interactivo.\n"
                        "Usa -c/--cat para volcar a stdout.\n",
                        argv[0]);
                doc_free(doc);
                return 1;
            }
        }

        /* ── iniciar renderer ncurses ── */
        Renderer *renderer = renderer_create(doc, first_file,
                                             buf->lines, buf->count);
        free(buf);  /* solo el struct, las líneas las posee el renderer */
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

        renderer_free(renderer);
    }

    return 0;
}
