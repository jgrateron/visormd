#include "highlight.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 * definición de un lenguaje: listas de palabras clave y tipos
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    const char  *name;            /* "c", "cpp", "java", "javascript" */
    const char **keywords;        /* array terminado en NULL */
    const char **types;           /* array terminado en NULL */
    int          has_preprocessor; /* preprocesador (#include, #define...) */
    int          hash_comment;    /* # es comentario de línea (Python, Ruby...) */
} LangDef;

/* ──────────────────────────────────────────────
 * C
 * ────────────────────────────────────────────── */
static const char *c_keywords[] = {
    "auto", "break", "case", "const", "continue",
    "default", "do", "else", "enum", "extern",
    "for", "goto", "if", "register", "return",
    "signed", "sizeof", "static", "struct", "switch",
    "typedef", "union", "unsigned", "volatile", "while",
    "restrict", "inline",
    NULL
};

static const char *c_types[] = {
    "char", "double", "float", "int", "long",
    "short", "void", "_Bool", "size_t", "ssize_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "FILE", "NULL", "bool",
    NULL
};

/* ──────────────────────────────────────────────
 * C++
 * ────────────────────────────────────────────── */
static const char *cpp_keywords[] = {
    "alignas", "alignof", "and", "and_eq", "asm",
    "auto", "bitand", "bitor", "bool", "break",
    "case", "catch", "char", "class", "compl",
    "concept", "const", "consteval", "constexpr", "constinit",
    "const_cast", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "dynamic_cast",
    "else", "enum", "explicit", "export", "extern",
    "false", "for", "friend", "goto", "if",
    "inline", "mutable", "namespace", "new", "noexcept",
    "not", "not_eq", "nullptr", "operator", "or",
    "or_eq", "override", "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return",
    "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this",
    "thread_local", "throw", "true", "try", "typedef",
    "typeid", "typename", "union", "unsigned", "using",
    "virtual", "volatile", "wchar_t", "while", "xor",
    "xor_eq",
    NULL
};

static const char *cpp_types[] = {
    "bool", "char", "char8_t", "char16_t", "char32_t",
    "double", "float", "int", "long", "short",
    "void", "wchar_t", "size_t", "ssize_t", "nullptr_t",
    "string", "wstring", "u16string", "u32string",
    "vector", "map", "unordered_map", "set", "unordered_set",
    "deque", "list", "forward_list", "array", "stack", "queue",
    "pair", "tuple", "optional", "variant", "any",
    "shared_ptr", "unique_ptr", "weak_ptr",
    "iostream", "istream", "ostream", "fstream",
    "cout", "cin", "cerr", "endl",
    "string_view", "span",
    "FILE", "NULL",
    NULL
};

/* ──────────────────────────────────────────────
 * Java
 * ────────────────────────────────────────────── */
static const char *java_keywords[] = {
    "abstract", "assert", "boolean", "break", "byte",
    "case", "catch", "char", "class", "const",
    "continue", "default", "do", "double", "else",
    "enum", "extends", "final", "finally", "float",
    "for", "goto", "if", "implements", "import",
    "instanceof", "int", "interface", "long", "native",
    "new", "package", "private", "protected", "public",
    "return", "short", "static", "strictfp", "super",
    "switch", "synchronized", "this", "throw", "throws",
    "transient", "try", "void", "volatile", "while",
    "true", "false", "null",
    "record", "sealed", "permits", "yield", "var",
    NULL
};

static const char *java_types[] = {
    "String", "Integer", "Boolean", "Byte", "Short",
    "Long", "Float", "Double", "Character", "Object",
    "Class", "Void", "Enum", "Number",
    "List", "Map", "Set", "Queue", "Deque",
    "ArrayList", "LinkedList", "Vector", "Stack",
    "HashMap", "TreeMap", "LinkedHashMap",
    "HashSet", "TreeSet", "LinkedHashSet",
    "Iterator", "Iterable", "Collection", "Collections",
    "Arrays", "Optional", "Stream",
    "StringBuilder", "StringBuffer",
    "Exception", "RuntimeException", "Throwable", "Error",
    "Comparable", "Serializable", "Cloneable",
    "Override", "SuppressWarnings", "Deprecated",
    NULL
};

/* ──────────────────────────────────────────────
 * JavaScript / TypeScript
 * ────────────────────────────────────────────── */
static const char *js_keywords[] = {
    "break", "case", "catch", "class", "const",
    "continue", "debugger", "default", "delete", "do",
    "else", "export", "extends", "finally", "for",
    "function", "if", "import", "in", "instanceof",
    "let", "new", "of", "return", "super",
    "switch", "this", "throw", "try", "typeof",
    "var", "void", "while", "with", "yield",
    "async", "await", "from", "as", "static",
    "get", "set", "enum",
    NULL
};

static const char *js_types[] = {
    "undefined", "null", "true", "false",
    "NaN", "Infinity",
    "Number", "String", "Boolean", "Array", "Object",
    "Function", "Date", "RegExp", "Error", "Map",
    "Set", "Promise", "Symbol", "BigInt",
    "JSON", "Math", "console",
    "Int8Array", "Uint8Array", "Int16Array", "Uint16Array",
    "Int32Array", "Uint32Array", "Float32Array", "Float64Array",
    "ArrayBuffer", "DataView",
    "window", "document", "globalThis",
    NULL
};

/* ──────────────────────────────────────────────
 * C#
 * ────────────────────────────────────────────── */
static const char *cs_keywords[] = {
    "abstract", "as", "base", "bool", "break",
    "byte", "case", "catch", "char", "checked",
    "class", "const", "continue", "decimal", "default",
    "delegate", "do", "double", "else", "enum",
    "event", "explicit", "extern", "false", "finally",
    "fixed", "float", "for", "foreach", "goto",
    "if", "implicit", "in", "int", "interface",
    "internal", "is", "lock", "long", "namespace",
    "new", "null", "object", "operator", "out",
    "override", "params", "private", "protected", "public",
    "readonly", "record", "ref", "return", "sbyte",
    "sealed", "short", "sizeof", "stackalloc", "static",
    "string", "struct", "switch", "this", "throw",
    "true", "try", "typeof", "uint", "ulong",
    "unchecked", "unsafe", "ushort", "using", "virtual",
    "void", "volatile", "while",
    /* contextuales */
    "add", "alias", "ascending", "async", "await",
    "by", "descending", "dynamic", "equals", "from",
    "get", "global", "group", "init", "into",
    "join", "let", "managed", "nameof", "nint",
    "not", "notnull", "nuint", "on", "or",
    "orderby", "partial", "remove", "select", "set",
    "unmanaged", "value", "var", "when", "where",
    "with", "yield",
    NULL
};

static const char *cs_types[] = {
    "object", "bool", "byte", "sbyte", "short", "ushort",
    "int", "uint", "long", "ulong", "float", "double",
    "decimal", "char", "void", "nint", "nuint",
    "DateTime", "TimeSpan", "DateTimeOffset", "Guid", "Uri",
    "List", "Dictionary", "HashSet", "Queue", "Stack",
    "LinkedList", "SortedList", "SortedSet", "SortedDictionary",
    "IEnumerable", "IEnumerator", "IList", "IDictionary",
    "ISet", "ICollection", "IReadOnlyList", "IReadOnlyDictionary",
    "IQueryable", "IGrouping", "ILookup",
    "Array", "ArrayList", "Hashtable", "BitArray",
    "StringBuilder", "Regex", "Match", "CultureInfo",
    "Task", "ValueTask", "TaskCompletionSource",
    "Exception", "ArgumentException", "ArgumentNullException",
    "InvalidOperationException", "NullReferenceException",
    "NotSupportedException", "NotImplementedException",
    "Stream", "FileStream", "MemoryStream", "BufferedStream",
    "File", "Directory", "Path", "FileInfo", "DirectoryInfo",
    "HttpClient", "HttpResponseMessage", "HttpContent",
    "Console", "Math", "Convert", "Enumerable", "Environment",
    "Tuple", "ValueTuple",
    "Nullable", "Lazy", "WeakReference",
    "Action", "Func", "Predicate", "EventHandler",
    "CancellationToken", "CancellationTokenSource",
    "XDocument", "XElement", "XAttribute", "XmlDocument",
    "DataTable", "DataSet", "SqlConnection", "SqlCommand",
    "string", "String",
    NULL
};

/* ──────────────────────────────────────────────
 * Visual Basic .NET
 * ────────────────────────────────────────────── */
static const char *vb_keywords[] = {
    "AddHandler", "AddressOf", "Alias", "And", "AndAlso",
    "As", "Async", "Await", "Boolean", "ByRef",
    "Byte", "ByVal", "Call", "Case", "Catch",
    "CBool", "CByte", "CChar", "CDate", "CDbl",
    "CDec", "Char", "CInt", "Class", "CLng",
    "CObj", "Const", "Continue", "CSByte", "CShort",
    "CSng", "CStr", "CType", "CUInt", "CULng",
    "CUShort", "Date", "Decimal", "Declare", "Default",
    "Delegate", "Dim", "DirectCast", "Do", "Double",
    "Each", "Else", "ElseIf", "End", "EndIf",
    "Enum", "Erase", "Error", "Event", "Exit",
    "False", "Finally", "For", "Friend", "Function",
    "Get", "GetType", "Global", "GoTo", "Handles",
    "If", "Implements", "Imports", "In", "Inherits",
    "Integer", "Interface", "Is", "IsNot", "Let",
    "Lib", "Like", "Long", "Loop", "Me",
    "Mod", "Module", "MustInherit", "MustOverride", "MyBase",
    "MyClass", "NameOf", "Narrowing", "New", "Next",
    "Not", "Nothing", "NotInheritable", "NotOverridable", "Object",
    "Of", "On", "Operator", "Option", "Optional",
    "Or", "OrElse", "Out", "Overloads", "Overridable",
    "Overrides", "ParamArray", "Partial", "Private", "Property",
    "Protected", "Public", "RaiseEvent", "ReadOnly", "ReDim",
    "Rem", "RemoveHandler", "Resume", "Return", "Select",
    "Set", "Shadows", "Shared", "Short", "Single",
    "Static", "Step", "Stop", "String", "Structure",
    "Sub", "SyncLock", "Then", "Throw", "To",
    "True", "Try", "TryCast", "TypeOf", "UInteger",
    "ULong", "UShort", "Using", "Variant", "Wend",
    "When", "While", "Widening", "With", "WithEvents",
    "WriteOnly", "Xor", "Yield",
    NULL
};

static const char *vb_types[] = {
    "Boolean", "Byte", "SByte", "Char", "Date",
    "Decimal", "Double", "Integer", "Long", "Object",
    "Short", "Single", "String", "UInteger", "ULong",
    "UShort",
    "List", "Dictionary", "HashSet", "Queue", "Stack",
    "IEnumerable", "IEnumerator", "IList", "IDictionary",
    "Task", "Exception", "EventArgs",
    "Console", "Math", "Convert", "Environment",
    "File", "Directory", "Path", "Stream",
    "DateTime", "TimeSpan", "Guid", "Uri",
    "SqlConnection", "SqlCommand", "DataTable", "DataSet",
    "XDocument", "XElement",
    "Regex", "Match", "StringBuilder",
    NULL
};

/* ──────────────────────────────────────────────
 * Python
 * ────────────────────────────────────────────── */
static const char *py_keywords[] = {
    "False", "None", "True", "and", "as", "assert",
    "async", "await", "break", "class", "continue",
    "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if",
    "import", "in", "is", "lambda", "nonlocal",
    "not", "or", "pass", "raise", "return",
    "try", "while", "with", "yield",
    "match", "case", "type",
    NULL
};

static const char *py_types[] = {
    "int", "float", "complex", "bool", "str",
    "bytes", "bytearray", "memoryview",
    "list", "tuple", "dict", "set", "frozenset",
    "range", "slice", "object",
    "Exception", "ValueError", "TypeError", "KeyError",
    "IndexError", "AttributeError", "RuntimeError",
    "StopIteration", "StopAsyncIteration",
    "NotImplementedError", "ImportError", "OSError",
    "FileNotFoundError", "PermissionError", "IsADirectoryError",
    "ConnectionError", "TimeoutError",
    "Warning", "DeprecationWarning", "FutureWarning",
    "type", "super", "property", "staticmethod", "classmethod",
    "any", "all", "enumerate", "filter", "map", "zip",
    "sorted", "reversed", "iter", "next", "open",
    "print", "input", "len", "abs", "round",
    "min", "max", "sum", "divmod", "pow",
    "isinstance", "issubclass", "hasattr", "getattr", "setattr",
    "callable", "repr", "str", "format",
    "bytes", "bytearray", "chr", "ord", "hex", "oct", "bin",
    "id", "hash", "dir", "vars",
    "IO", "TextIO", "BinaryIO",
    "Path", "PathLike",
    "self", "cls",
    NULL
};

/* JSON */
static const char *json_keywords[] = {
    "true", "false", "null",
    NULL
};

static const char *json_types[] = {
    NULL
};

/* ──────────────────────────────────────────────
 * tabla de lenguajes soportados
 * ────────────────────────────────────────────── */
static const LangDef languages[] = {
    { "c",          c_keywords,   c_types,   1, 0 },
    { "cpp",        cpp_keywords, cpp_types, 1, 0 },
    { "c++",        cpp_keywords, cpp_types, 1, 0 },
    { "cc",         cpp_keywords, cpp_types, 1, 0 },
    { "cxx",        cpp_keywords, cpp_types, 1, 0 },
    { "h",          c_keywords,   c_types,   1, 0 },
    { "hpp",        cpp_keywords, cpp_types, 1, 0 },
    { "java",       java_keywords,java_types, 0, 0 },
    { "javascript", js_keywords,  js_types,   0, 0 },
    { "js",         js_keywords,  js_types,   0, 0 },
    { "ts",         js_keywords,  js_types,   0, 0 },
    { "typescript", js_keywords,  js_types,   0, 0 },
    { "cs",         cs_keywords,  cs_types,   0, 0 },
    { "csharp",     cs_keywords,  cs_types,   0, 0 },
    { "c#",         cs_keywords,  cs_types,   0, 0 },
    { "vb",         vb_keywords,  vb_types,   0, 0 },
    { "vbnet",      vb_keywords,  vb_types,   0, 0 },
    { "vb.net",     vb_keywords,  vb_types,   0, 0 },
    { "visualbasic",vb_keywords,  vb_types,   0, 0 },
    { "json",       json_keywords,json_types, 0, 0 },
    { "python",     py_keywords,  py_types,   0, 1 },
    { "py",         py_keywords,  py_types,   0, 1 },
    { NULL, NULL, NULL, 0, 0 }
};

/* ──────────────────────────────────────────────
 * helper: buscar un string en un array NULL-terminado
 * ────────────────────────────────────────────── */
static int str_in_array(const char *word, const char **array) {
    if (!array || !word) return 0;
    for (int i = 0; array[i]; i++) {
        if (strcmp(word, array[i]) == 0) return 1;
    }
    return 0;
}

/* ──────────────────────────────────────────────
 * helper: ¿es esta línea una directiva de preprocesador?
 * detecta # al principio (ignorando espacios en blanco)
 * ────────────────────────────────────────────── */
static int is_preprocessor_line(const char *text) {
    while (*text == ' ' || *text == '\t') text++;
    return (*text == '#');
}

/* ──────────────────────────────────────────────
 * helper: ¿el carácter puede ser parte de un número?
 * ────────────────────────────────────────────── */
static int is_hex_digit(char c) {
    return isdigit((unsigned char)c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int is_octal_digit(char c) {
    return (c >= '0' && c <= '7');
}

static int is_bin_digit(char c) {
    return (c == '0' || c == '1');
}

/* ──────────────────────────────────────────────
 * helper: añadir un span con un string recién copiado
 * ────────────────────────────────────────────── */
static void emit_span(ParsedLine *line, const char *text,
                      SpanType type) {
    if (!text || !*text) return;
    /* add_span es una función static en parser.c; como no tenemos
     * acceso directo, la replicamos aquí. Usamos la misma lógica. */
    Span s;
    s.text = strdup(text);
    s.type = type;
    s.url  = NULL;

    if (line->span_count >= line->span_capacity) {
        line->span_capacity = line->span_capacity ? line->span_capacity * 2 : 8;
        line->spans = realloc(line->spans,
                              sizeof(Span) * (size_t)line->span_capacity);
    }
    line->spans[line->span_count++] = s;
}

/* ──────────────────────────────────────────────
 * buffer de caracteres para acumular texto normal
 * ────────────────────────────────────────────── */
typedef struct {
    char *data;
    int   len;
    int   cap;
} CBuf;

static void cb_init(CBuf *cb) {
    cb->data = NULL;
    cb->len  = 0;
    cb->cap  = 0;
}

static void cb_put(CBuf *cb, char c) {
    if (cb->len >= cb->cap) {
        cb->cap = cb->cap ? cb->cap * 2 : 64;
        cb->data = realloc(cb->data, (size_t)cb->cap);
    }
    cb->data[cb->len++] = c;
}

static char *cb_extract(CBuf *cb) {
    if (!cb->data) return strdup("");
    cb_put(cb, '\0');
    char *s = cb->data;
    cb->data = NULL;
    cb->len  = 0;
    cb->cap  = 0;
    return s;
}

static void cb_flush(CBuf *cb, ParsedLine *line, SpanType type) {
    if (cb->len == 0) return;
    char *text = cb_extract(cb);
    emit_span(line, text, type);
    free(text);
}

/* ══════════════════════════════════════════════════════════════
 * tokenizador principal
 * ══════════════════════════════════════════════════════════════ */

void highlight_state_init(HighlightState *st) {
    st->in_block_comment = 0;
    st->in_triple_quote = 0;
}

int highlight_supported(const char *lang) {
    if (!lang || !*lang) return 0;
    for (int i = 0; languages[i].name; i++) {
        if (strcmp(lang, languages[i].name) == 0) return 1;
    }
    return 0;
}

static const LangDef *find_lang(const char *lang) {
    if (!lang) return NULL;
    for (int i = 0; languages[i].name; i++) {
        if (strcmp(lang, languages[i].name) == 0)
            return &languages[i];
    }
    return NULL;
}

int highlight_line(ParsedLine *line, const char *text,
                   const char *lang, HighlightState *st) {
    const LangDef *ld = find_lang(lang);
    if (!ld || !line) return -1;

    int   len = (int)strlen(text);
    int   i   = 0;
    CBuf  buf;
    cb_init(&buf);

    /* ── si venimos de un comentario multilínea abierto ── */
    if (st->in_block_comment) {
        /* buscar el cierre * / */
        const char *end = strstr(text, "*/");
        if (end) {
            /* desde inicio hasta * / inclusive es comentario */
            int comment_end = (int)(end - text) + 2;
            char *comment = strndup(text, (size_t)comment_end);
            emit_span(line, comment, SPAN_KW_COMMENT);
            free(comment);
            i = comment_end;
            st->in_block_comment = 0;
        } else {
            /* toda la línea es comentario */
            emit_span(line, text, SPAN_KW_COMMENT);
            return 0;
        }
    }

    /* ── si venimos de un string triple abierto (Python) ── */
    if (st->in_triple_quote) {
        const char *closer = (st->in_triple_quote == 1) ? "\"\"\"" : "'''";
        const char *end = strstr(text, closer);
        if (end) {
            int str_end = (int)(end - text) + 3;
            char *s = strndup(text, (size_t)str_end);
            emit_span(line, s, SPAN_KW_STRING);
            free(s);
            i = str_end;
            st->in_triple_quote = 0;
        } else {
            /* toda la línea es parte del string */
            emit_span(line, text, SPAN_KW_STRING);
            return 0;
        }
    }

    /* ── preprocesador (# al inicio de línea) ── */
    if (ld->has_preprocessor && is_preprocessor_line(text)) {
        emit_span(line, text, SPAN_KW_PREPROC);
        return 0;
    }

    /* ── tokenizar carácter por carácter ── */
    while (i < len) {
        /* ── espacios y tabs → texto normal ── */
        if (text[i] == ' ' || text[i] == '\t') {
            cb_put(&buf, text[i++]);
            continue;
        }

        /* ── comentario de línea: // ── */
        if (text[i] == '/' && text[i + 1] == '/') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, text + i, SPAN_KW_COMMENT);
            return 0;  /* resto de la línea es comentario */
        }

        /* ── comentario de línea: # (Python, Ruby...) ── */
        if (text[i] == '#' && ld->hash_comment) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, text + i, SPAN_KW_COMMENT);
            return 0;  /* resto de la línea es comentario */
        }

        /* ── comentario multilinea apertura: / * ── */
        if (text[i] == '/' && text[i + 1] == '*') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i += 2;
            const char *end = strstr(text + i, "*/");
            if (end) {
                i = (int)(end - text) + 2;
                char *comment = strndup(text + start, (size_t)(i - start));
                emit_span(line, comment, SPAN_KW_COMMENT);
                free(comment);
            } else {
                /* multilínea que continúa en la siguiente línea */
                emit_span(line, text + start, SPAN_KW_COMMENT);
                st->in_block_comment = 1;
                return 0;
            }
            continue;
        }

        /* ── string triple-comilla: """...""" o '''...''' (Python) ── */
        if ((text[i] == '"' && text[i + 1] == '"' && text[i + 2] == '"') ||
            (text[i] == '\'' && text[i + 1] == '\'' && text[i + 2] == '\'')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            const char *closer = (text[i] == '"') ? "\"\"\"" : "'''";
            int start = i;
            i += 3;
            const char *end = strstr(text + i, closer);
            if (end) {
                i = (int)(end - text) + 3;
                char *s = strndup(text + start, (size_t)(i - start));
                emit_span(line, s, SPAN_KW_STRING);
                free(s);
            } else {
                /* multilínea: continúa en la siguiente línea */
                emit_span(line, text + start, SPAN_KW_STRING);
                st->in_triple_quote = (text[start] == '"') ? 1 : 2;
                return 0;
            }
            continue;
        }

        /* ── string con comillas dobles: "..." ── */
        if (text[i] == '"') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;  /* comilla de cierre */
            char *s = strndup(text + start, (size_t)(i - start));
            emit_span(line, s, SPAN_KW_STRING);
            free(s);
            continue;
        }

        /* ── string con comillas simples: '...' (char en C/C++/Java,
         *   string en JS, pero se resalta igual) ── */
        if (text[i] == '\'') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && text[i] != '\'') {
                if (text[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            char *s = strndup(text + start, (size_t)(i - start));
            emit_span(line, s, SPAN_KW_STRING);
            free(s);
            continue;
        }

        /* ── template literal JS/TS: `...`  (solo para JS) ── */
        if (text[i] == '`' &&
            (strcmp(ld->name, "javascript") == 0 ||
             strcmp(ld->name, "js") == 0 ||
             strcmp(ld->name, "ts") == 0 ||
             strcmp(ld->name, "typescript") == 0)) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && text[i] != '`') {
                if (text[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            char *s = strndup(text + start, (size_t)(i - start));
            emit_span(line, s, SPAN_KW_STRING);
            free(s);
            continue;
        }

        /* ── número: dígito, o . seguido de dígito, o 0x, 0b, 0o ── */
        if (isdigit((unsigned char)text[i]) ||
            (text[i] == '.' && isdigit((unsigned char)text[i + 1]))) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;

            /* prefijo de base: 0x, 0X, 0b, 0B, 0o, 0O */
            if (text[i] == '0' && i + 1 < len) {
                char nx = (char)tolower((unsigned char)text[i + 1]);
                if (nx == 'x') {
                    i += 2;
                    while (i < len && (is_hex_digit(text[i]) || text[i] == '_')) i++;
                    goto num_suffix;
                }
                if (nx == 'b') {
                    i += 2;
                    while (i < len && (is_bin_digit(text[i]) || text[i] == '_')) i++;
                    goto num_suffix;
                }
                if (nx == 'o') {
                    i += 2;
                    while (i < len && (is_octal_digit(text[i]) || text[i] == '_')) i++;
                    goto num_suffix;
                }
            }

            /* parte entera */
            while (i < len && (isdigit((unsigned char)text[i]) || text[i] == '_')) i++;

            /* parte fraccionaria */
            if (i < len && text[i] == '.') {
                i++;
                while (i < len && (isdigit((unsigned char)text[i]) || text[i] == '_')) i++;
            }

            /* exponente */
            if (i < len && (text[i] == 'e' || text[i] == 'E')) {
                i++;
                if (i < len && (text[i] == '+' || text[i] == '-')) i++;
                while (i < len && (isdigit((unsigned char)text[i]) || text[i] == '_')) i++;
            }

        num_suffix:
            /* sufijos C/C++/Java: f F l L u U ll LL ul UL etc */
            while (i < len && (text[i] == 'f' || text[i] == 'F' ||
                   text[i] == 'l' || text[i] == 'L' ||
                   text[i] == 'u' || text[i] == 'U')) i++;

            /* sufijo JS: n (BigInt) */
            if (i < len && text[i] == 'n') i++;

            /* sufijo Python: j J (números complejos) */
            if (i < len && (text[i] == 'j' || text[i] == 'J')) i++;

            char *num = strndup(text + start, (size_t)(i - start));
            emit_span(line, num, SPAN_KW_NUMBER);
            free(num);
            continue;
        }

        /* ── identificador / palabra clave ── */
        if (isalpha((unsigned char)text[i]) || text[i] == '_') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            while (i < len &&
                   (isalnum((unsigned char)text[i]) || text[i] == '_'))
                i++;
            char *word = strndup(text + start, (size_t)(i - start));

            if (str_in_array(word, ld->keywords))
                emit_span(line, word, SPAN_KW_KEYWORD);
            else if (str_in_array(word, ld->types))
                emit_span(line, word, SPAN_KW_TYPE);
            else
                emit_span(line, word, SPAN_KW_NORMAL);

            free(word);
            continue;
        }

        /* ── cualquier otro carácter: operadores, puntuación, etc. ── */
        cb_put(&buf, text[i++]);
    }

    cb_flush(&buf, line, SPAN_KW_NORMAL);
    return 0;
}
