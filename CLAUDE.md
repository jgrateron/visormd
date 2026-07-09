# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Develop

```bash
make                  # Build the visormd binary
make clean            # Remove obj/ and binary
make install          # Install to /usr/local/bin (requires sudo typically)
```

The binary is a standalone executable with no runtime dependencies beyond `libncursesw`.

To quickly test rendering with a sample file:

```bash
TERM=xterm-256color LANG=C.UTF-8 timeout 1 ./visormd test.md; echo "exit: $?"
```

### Test suite

```bash
make test           # Build and run all verification tests
make clean test     # Clean rebuild and test
```

Test Markdown inputs (`test/*.md`) and their expected plain-text outputs (`test/*_expected.txt`) live in the `test/` directory. The expected files were generated with `--cat` redirected to a file (no ANSI codes — `cat_renderer` suppresses them when stdout is not a tty). The `test` target compares each `test*.md` against its `test*_expected.txt` via `diff`.

To regenerate expected outputs after a rendering change:

```bash
for f in test/test.md test/test_emoji.md test/test_table.md test/test_table2.md test/test_user.md test/test_utf8.md test/test_wide.md test/test_lista.md test/test_underscore.md test/test_blockquote.md; do
    base="${f%.md}"
    TERM=xterm-256color LANG=C.UTF-8 ./visormd --cat "$f" > "${base}_expected.txt"
done
```

**Test file coverage:**

| File | What it exercises |
|------|-------------------|
| `test.md` | Headings H1–H6, bold, italic, bold-italic, inline code, links, blockquotes, fenced code blocks, unordered/ordered lists, horizontal rules, paragraph wrapping |
| `test_emoji.md` | Emoji in headings and body text, tables with inline format (`**bold**`), mixed emoji + markdown |
| `test_table.md` | Basic table with Unicode box-drawing borders, no inline formatting in cells |
| `test_table2.md` | Table alignment (left/center/right), inline bold/italic/code/links in cells, minimal table without outer pipes |
| `test_user.md` | Single standalone table with backtick-wrapped code in cells, long cell text wrapping across multiple visual lines |
| `test_utf8.md` | Spanish accented characters (áéíóúñü), mixed with bold/italic/inline code/blockquote/fenced bash block, links with accents in URL, escaped heading |
| `test_wide.md` | Table with very long cell content forcing multi-line wrapping per row, CJK/wide characters |
| `test_lista.md` | Unordered list items with bold terms, inline code, and long wrapped lines |
| `test_underscore.md` | Bold with `__text__`, italic with `_text_`, bold+italic with `___text___` and `__*text*__`, snake_case preservation |
| `test_blockquote.md` | Single-level `>`, nested `>>` and `>>>` blockquotes, empty blockquote lines with `>` |

## Architecture

VisorMD is a terminal-based interactive Markdown viewer, written in C11 with ncursesw. It reads a single Markdown file and presents it in a scrollable, color-coded terminal UI with vim-like keybindings.

### Data flow (pipeline)

```
File / stdin → TextBuffer (raw lines) → Parser (Document/ParsedLine/Spans) → Renderer (ncurses)
                                                                            → cat_renderer (stdout + ANSI)
```

The default mode is interactive (ncurses). With `-c`/`--cat`, the pipeline branches to `cat_renderer` which dumps the document to stdout with ANSI escape codes and exits immediately — no ncurses dependency in that path.

1. **`src/buffer.c`** — `TextBuffer`: reads a file (`buffer_load_file`) or stdin (`buffer_load_stdin`) into a dynamic array of raw UTF-8 strings (one per line). Owns the I/O and is discarded after parsing.

2. **`src/parser.c`** — Converts raw lines into a `Document` tree:
   - **Line-level** (`detect_line_type`): classifies each line as a heading (H1–H6), code block fence/content, horizontal rule, blockquote, unordered/ordered list, empty line, or paragraph. Handles code block state tracking across lines.
   - **Inline** (`parse_inline`): within non-code-block lines, identifies spans of `**bold**`/`__bold__`, `*italic*`/`_italic_`, `` `code` ``, and `[link text](url)`.
   - **List markers** are extracted as separate `SPAN_LIST_MARKER` spans so the renderer can color them distinctly.
   - Output is a `Document` containing a dynamic array of `ParsedLine`, each with a `LineType`, an indent level, and an array of `Span` structs (each with text, `SpanType`, and optional URL).

3. **`src/renderer.c`** — ncurses-based interactive viewer:
   - Converts UTF-8 to wide characters (`mbrtowc`) and uses `wcwidth` for correct column accounting — this is how emoji (width 2), CJK (width 2), and accented characters render correctly.
   - Color pairs are defined in `init_colors()`; `span_attr()` maps `SpanType` + `LineType` to ncurses attributes.
   - Scroll state is tracked as `(scroll_line, scroll_skip)` — a line index plus how many wrapped sub-rows to skip within that line. This handles line wrapping at any terminal width.
   - `renderer_draw` iterates from the scroll position, renders each source line (with wrapping) until the screen is filled, then draws the status bar.
   - Terminal resize is handled via `KEY_RESIZE` → `renderer_resize`, which re-measures and re-clamps scroll state.

4. **`src/cat_renderer.c`** — Non-interactive stdout renderer (triggered by `-c`/`--cat`):
   - Iterates the parsed `Document` and writes each `ParsedLine` to stdout.
   - Maps `SpanType` + `LineType` to ANSI escape codes (bold, italic, underline, 16 SGR colors) — same logic as `span_attr()` in renderer.c but for ANSI terminals.
   - Tables are rendered with Unicode box-drawing characters and cell wrapping (same column-width algorithm as the ncurses renderer).
   - Checks `isatty(STDOUT_FILENO)`: if stdout is not a terminal (pipe/redirect), ANSI codes are suppressed — plain text only.
   - Does not depend on ncurses; only the parser, `<stdio.h>`, `<wchar.h>`, and `<sys/ioctl.h>`.

5. **`src/theme.c`** — Color theme system:
   - Defines 8 named theme palettes as `static const Theme` structs, each specifying fg/bg for all 16 color pairs.
   - Config I/O: reads/writes `theme=<id>` in `$HOME/.config/visormd/config` (with `$XDG_CONFIG_HOME` support).
   - Theme selector overlay (triggered by F2) is implemented as a static function in `renderer.c` since it needs intimate access to ncurses windows.

6. **`src/main.c`** — Entry point: argument parsing (`-c`/`--cat` for stdout dump, `-h` for help, or a single filename), locale setup (tries `""`, `C.UTF-8`, `en_US.UTF-8` in that order). Auto-detects non-TTY stdin via `isatty(STDIN_FILENO)` and switches to cat mode transparently — so `cat file.md | visormd` works without `-c`. When `cat_mode` is set, calls `cat_render()` and exits; otherwise wires the pipeline through the ncurses renderer and runs the input loop until `q`.

### Keybindings (hardcoded in `renderer_handle_input`)

| Key | Action |
|-----|--------|
| `q` | Quit |
| `j` / `↓` | Scroll down one line |
| `k` / `↑` | Scroll up one line |
| Space / PgDn | Page down |
| `b` / PgUp | Page up |
| `g` / Home | Go to top |
| `G` / End | Go to bottom |
| `n` | Toggle line numbers |
| `w` | Toggle word wrap |
| `F2` | Open theme selector overlay |

### Color themes

8 themes are defined in `src/theme.c`: Default, Monochrome, Solarized Dark, Solarized Light, Nord, Gruvbox Dark, Dracula, One Dark. The selected theme is persisted to `$HOME/.config/visormd/config` (respects `$XDG_CONFIG_HOME`). Color pair indices are shared between `theme.h` (macros) and `renderer.c`.

### Language & locale

All user-facing strings (usage, status bar) are in Spanish. The program requires a UTF-8 locale; `main.c` attempts several fallbacks. The link library is `-lncursesw` (wide-character ncurses), not plain `-lncurses`.
