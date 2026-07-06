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

There is no test suite or lint configuration.

## Architecture

VisorMD is a terminal-based interactive Markdown viewer, written in C11 with ncursesw. It reads a single Markdown file and presents it in a scrollable, color-coded terminal UI with vim-like keybindings.

### Data flow (pipeline)

```
File → TextBuffer (raw lines) → Parser (Document/ParsedLine/Spans) → Renderer (ncurses)
```

1. **`src/buffer.c`** — `TextBuffer`: reads a file into a dynamic array of raw UTF-8 strings (one per line). Owns the file I/O and is discarded after parsing.

2. **`src/parser.c`** — Converts raw lines into a `Document` tree:
   - **Line-level** (`detect_line_type`): classifies each line as a heading (H1–H6), code block fence/content, horizontal rule, blockquote, unordered/ordered list, empty line, or paragraph. Handles code block state tracking across lines.
   - **Inline** (`parse_inline`): within non-code-block lines, identifies spans of `**bold**`, `*italic*`, `` `code` ``, and `[link text](url)`.
   - **List markers** are extracted as separate `SPAN_LIST_MARKER` spans so the renderer can color them distinctly.
   - Output is a `Document` containing a dynamic array of `ParsedLine`, each with a `LineType`, an indent level, and an array of `Span` structs (each with text, `SpanType`, and optional URL).

3. **`src/renderer.c`** — ncurses-based interactive viewer:
   - Converts UTF-8 to wide characters (`mbrtowc`) and uses `wcwidth` for correct column accounting — this is how emoji (width 2), CJK (width 2), and accented characters render correctly.
   - Color pairs are defined in `init_colors()`; `span_attr()` maps `SpanType` + `LineType` to ncurses attributes.
   - Scroll state is tracked as `(scroll_line, scroll_skip)` — a line index plus how many wrapped sub-rows to skip within that line. This handles line wrapping at any terminal width.
   - `renderer_draw` iterates from the scroll position, renders each source line (with wrapping) until the screen is filled, then draws the status bar.
   - Terminal resize is handled via `KEY_RESIZE` → `renderer_resize`, which re-measures and re-clamps scroll state.

4. **`src/theme.c`** — Color theme system:
   - Defines 8 named theme palettes as `static const Theme` structs, each specifying fg/bg for all 16 color pairs.
   - Config I/O: reads/writes `theme=<id>` in `$HOME/.config/visormd/config` (with `$XDG_CONFIG_HOME` support).
   - Theme selector overlay (triggered by F2) is implemented as a static function in `renderer.c` since it needs intimate access to ncurses windows.

5. **`src/main.c`** — Entry point: argument parsing (single filename or `-h`), locale setup (tries `""`, `C.UTF-8`, `en_US.UTF-8` in that order), then wires the pipeline and runs the input loop until `q`.

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
