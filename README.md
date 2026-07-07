# VisorMD

A terminal-based interactive Markdown viewer written in C11 with ncursesw. It reads a Markdown file and displays it in a scrollable, syntax-highlighted terminal UI with vim-like keybindings.

![C11](https://img.shields.io/badge/language-C11-blue)
![ncursesw](https://img.shields.io/badge/library-ncursesw-green)

## Features

- **Headings** (H1–H6) with distinct colors
- **Inline formatting** — bold, italic, `code`, and [links](https://example.com)
- **Tables** with box-drawing borders (`┌┬┐├┼┤└┴┘`), column alignment (`:---`, `:---:`, `---:`), and inline formatting inside cells
- **Code blocks** (fenced with ` ``` ` or `~~~`)
- **Blockquotes**, horizontal rules, unordered and ordered lists
- **UTF-8 support** — emoji, CJK characters, and accented text with correct column width (via `wcwidth`)
- **8 color themes** — Default, Monochrome, Solarized Dark/Light, Nord, Gruvbox Dark, Dracula, One Dark
- **Vim-like navigation** — `j`/`k`, `gg`/`G`, space/page-up/page-down
- **Smart word-wrap** — words stay whole when wrapping to the next line (togglable with `w`)
- **Responsive** — handles terminal resize, line wrapping, and proportional column scaling for wide tables

## Requirements

- **libncursesw** (wide-character ncurses)
- A UTF-8 locale (e.g., `C.UTF-8`, `en_US.UTF-8`)
- GCC or compatible C11 compiler

### Install dependencies

**Debian / Ubuntu:**
```bash
sudo apt install libncursesw5-dev
```

**Fedora:**
```bash
sudo dnf install ncursesw-devel
```

**Arch:**
```bash
sudo pacman -S ncurses
```

**macOS:**
```bash
brew install ncurses
```

## Build

```bash
make                  # Build the visormd binary
make clean            # Remove object files and binary
make install          # Install to /usr/local/bin
```

The binary is a standalone executable with no runtime dependencies beyond `libncursesw`.

## Usage

```bash
visormd [OPCIONES] <file.md>
```

| Option | Description |
|--------|-------------|
| `-c`, `--cat` | Dump rendered Markdown to stdout with ANSI colors and exit (no ncurses) |
| `-h`, `--help` | Show the help message |

Without options, the program starts in interactive mode with ncurses.

### Keybindings

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
| `w` | Toggle word-wrap (default: on, words stay whole) |
| `F2` | Open theme selector |

### Quick test

```bash
TERM=xterm-256color LANG=C.UTF-8 timeout 1 ./visormd test/test.md; echo "exit: $?"
```

## Architecture

```
File → TextBuffer (raw lines) → Parser (Document/ParsedLine/Spans) → Renderer (ncurses)
                                                                   → cat_renderer (stdout + ANSI)
```

- **`src/buffer.c`** — Reads a file into a dynamic array of raw UTF-8 strings.
- **`src/parser.c`** — Parses Markdown into a `Document` tree: classifies line types, extracts inline spans (bold, italic, code, links), parses table blocks with column alignment.
- **`src/renderer.c`** — ncursesw interactive viewer: renders spans with color attributes, handles line wrapping, scroll state, terminal resize, and the theme selector overlay.
- **`src/cat_renderer.c`** — Non-interactive stdout renderer used by `--cat`/`-c`: iterates the parsed document and emits plain text with ANSI escape codes (disabled when stdout is not a TTY).
- **`src/theme.c`** — 8 named color palettes with config persistence in `$HOME/.config/visormd/config` (respects `$XDG_CONFIG_HOME`).
- **`src/main.c`** — Entry point: argument parsing (filename, `-c`/`--cat`, `-h`), locale setup, wires the pipeline, runs either the cat renderer or the interactive loop.

## Configuration

The selected theme is saved to `$XDG_CONFIG_HOME/visormd/config` (or `~/.config/visormd/config`). The file contains a single line:

```
theme=gruvbox
```

Available theme IDs: `default`, `monochrome`, `solarized-dark`, `solarized-light`, `nord`, `gruvbox`, `dracula`, `one-dark`.

## Table support

VisorMD renders GitHub-flavored pipe tables with full box-drawing borders:

```
┌──────────────────┬────────────────────────────────┐
│    Left align    │          Right align           │
├──────────────────┼────────────────────────────────┤
│ **bold**         │                       42.5    │
├──────────────────┼────────────────────────────────┤
│ *italic*         │                   `code`      │
└──────────────────┴────────────────────────────────┘
```

- Column alignment: `:---` (left), `:---:` (center), `---:` (right)
- Headers are centered and bold
- Inline formatting works inside cells
- Wide tables scale proportionally to fit the terminal
- Content that overflows is truncated with `…`

## License

MIT
