#include "buffer.h"
#include "cat_renderer.h"
#include "parser.h"
#include "renderer.h"
#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_RECURSIVE_DEPTH 10
#define MAX_RECURSIVE_DEPTH     64

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [OPCIONES] [archivo.md | directorio | glob ...]\n"
            "\n"
            "Visor interactivo de Markdown para terminal.\n"
            "Acepta múltiples archivos, directorios (busca *.md) y patrones glob.\n"
            "\n"
            "Opciones:\n"
            "  -c, --cat        Volcar el contenido renderizado a stdout y salir\n"
            "  -R, --recursive  Buscar archivos *.md recursivamente en directorios\n"
            "                   (profundidad por defecto: %d, máx: %d)\n"
            "                   Usa -R <n> para limitar, ej: -R 3\n"
            "  -h, --help       Mostrar esta ayuda\n"
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
            "  %s -R docs/                # búsqueda recursiva (profundidad %d)\n"
            "  %s -R 2 docs/              # recursivo con profundidad 2\n"
            "  %s *.md                    # expansión shell de glob\n"
            "  %s README.md CHANGES.md    # múltiples archivos\n"
            "  cat README.md | %s         # pipe → interactivo\n"
            "  cat README.md | %s -c      # pipe → volcar a consola\n",
            prog,
            DEFAULT_RECURSIVE_DEPTH, MAX_RECURSIVE_DEPTH,
            prog, prog, prog, prog,
            DEFAULT_RECURSIVE_DEPTH,
            prog, prog, prog, prog, prog);
}

/* ──────────────────────────────────────────────
 * comparador para ordenar strings (qsort)
 * ────────────────────────────────────────────── */
static int cmpstring(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* ──────────────────────────────────────────────
 * construir ruta: dir + "/" + name
 * evita doble slash si dir ya termina en /
 * ────────────────────────────────────────────── */
static char *build_path(const char *dir, const char *name) {
    size_t dl = strlen(dir);
    size_t nl = strlen(name);
    int    has_slash = (dl > 0 && dir[dl - 1] == '/');
    size_t total = dl + (has_slash ? 0 : 1) + nl + 1;
    char *path = malloc(total);
    if (has_slash)
        snprintf(path, total, "%s%s", dir, name);
    else
        snprintf(path, total, "%s/%s", dir, name);
    return path;
}

/* ──────────────────────────────────────────────
 * escanear un directorio recursivamente en busca
 * de archivos *.md hasta depth niveles
 * ────────────────────────────────────────────── */
static char **scan_dir_recursive(const char *dirpath, int depth,
                                  int *out_count) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        *out_count = 0;
        return NULL;
    }

    int cap = 0, cnt = 0;
    char **files   = NULL;
    char **subdirs = NULL;
    int    sub_cap = 0, sub_cnt = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        /* saltar . .. y ocultos */
        if (name[0] == '.') continue;

        size_t nl = strlen(name);
        char *full = build_path(dirpath, name);
        if (!full) continue;

        /* ¿archivo .md? */
        if (nl >= 3 && strcmp(name + nl - 3, ".md") == 0) {
            if (cnt >= cap) {
                cap = cap ? cap * 2 : 16;
                files = realloc(files, sizeof(char *) * (size_t)(cap + 1));
                if (!files) { free(full); goto error; }
            }
            files[cnt++] = full;
            continue;
        }

        /* ¿subdirectorio? solo si queda profundidad */
        if (depth > 1) {
            struct stat st;
            if (lstat(full, &st) == 0 && S_ISDIR(st.st_mode) &&
                !S_ISLNK(st.st_mode)) {
                if (sub_cnt >= sub_cap) {
                    sub_cap = sub_cap ? sub_cap * 2 : 16;
                    subdirs = realloc(subdirs,
                                      sizeof(char *) * (size_t)(sub_cap + 1));
                    if (!subdirs) { free(full); goto error; }
                }
                subdirs[sub_cnt++] = full;
                continue;
            }
        }

        free(full);
    }
    closedir(dir);

    /* ── recurrir en subdirectorios ── */
    for (int i = 0; i < sub_cnt; i++) {
        int   sc = 0;
        char **sf = scan_dir_recursive(subdirs[i], depth - 1, &sc);
        free(subdirs[i]);
        if (sf && sc > 0) {
            for (int j = 0; j < sc; j++) {
                if (cnt >= cap) {
                    cap = cap ? cap * 2 : 16;
                    files = realloc(files,
                                    sizeof(char *) * (size_t)(cap + 1));
                    if (!files) { free(sf); free(subdirs); goto error; }
                }
                files[cnt++] = sf[j];
            }
        }
        free(sf);
    }
    free(subdirs);

    if (cnt > 0) {
        qsort(files, (size_t)cnt, sizeof(char *), cmpstring);
        files[cnt] = NULL;
        *out_count = cnt;
        return files;
    }
    free(files);
    *out_count = 0;
    return NULL;

error:
    closedir(dir);
    if (files) { for (int i = 0; i < cnt; i++) free(files[i]); free(files); }
    if (subdirs) { for (int i = 0; i < sub_cnt; i++) free(subdirs[i]); free(subdirs); }
    *out_count = 0;
    return NULL;
}

/* ──────────────────────────────────────────────
 * expandir un argumento en una lista de archivos
 * - si es directorio → buscar *.md dentro
 *   (recursivo si max_depth > 1)
 * - si contiene metacaracteres glob → glob()
 * - si es archivo regular → usarlo tal cual
 * retorna NULL si no se encontró nada
 * ────────────────────────────────────────────── */
static char **expand_arg(const char *arg, int max_depth, int *out_count) {
    struct stat st;
    char **files = NULL;
    *out_count = 0;

    /* ¿es un directorio? */
    if (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (max_depth > 1)
            return scan_dir_recursive(arg, max_depth, out_count);

        /* escaneo no recursivo (comportamiento actual) */
        DIR *dir = opendir(arg);
        if (!dir) return NULL;

        int cap = 0, cnt = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            size_t nl = strlen(name);
            if (nl < 3) continue;
            if (strcmp(name + nl - 3, ".md") != 0) continue;
            if (name[0] == '.') continue;

            if (cnt >= cap) {
                cap = cap ? cap * 2 : 16;
                files = realloc(files, sizeof(char *) * (size_t)(cap + 1));
            }
            files[cnt++] = build_path(arg, name);
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
    int cat_mode  = 0;
    int max_depth = 1;   /* 1 = sin recursión, >1 = profundidad máxima */

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
        if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--recursive") == 0) {
            max_depth = DEFAULT_RECURSIVE_DEPTH;
            /* ¿el siguiente argumento es un número? */
            if (i + 1 < argc) {
                char *end = NULL;
                errno = 0;
                long val = strtol(argv[i + 1], &end, 10);
                if (end && *end == '\0' && errno == 0) {
                    if (val < 1) val = 1;
                    if (val > MAX_RECURSIVE_DEPTH) val = MAX_RECURSIVE_DEPTH;
                    max_depth = (int)val;
                    i++;  /* consumir el argumento numérico */
                }
            }
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
        char **expanded = expand_arg(file_args[i], max_depth, &n);
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
