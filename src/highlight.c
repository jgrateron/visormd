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
    int          is_xml;          /* lenguaje de markup XML/HTML */
    int          has_dash_comment; /* -- es comentario de línea (SQL) */
    int          is_gherkin;      /* Gherkin: @tags, <placeholders>, * pasos */
    int          is_properties;   /* Properties/INI: clave=valor, # y ! comentarios */
    int          is_yaml;         /* YAML: clave: valor, comentarios, anclas... */
    int          is_gitignore;    /* Gitignore: patrones, negación !, # comentarios */
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
 * Kotlin
 * ────────────────────────────────────────────── */
static const char *kotlin_keywords[] = {
    "fun", "val", "var", "class", "object",
    "interface", "data", "sealed", "enum", "open",
    "abstract", "override", "private", "protected",
    "public", "internal", "companion", "inner",
    "constructor", "init", "this", "super",
    "if", "else", "when", "for", "while",
    "do", "return", "break", "continue",
    "try", "catch", "finally", "throw",
    "true", "false", "null",
    "is", "as", "in",
    "package", "import", "typealias",
    "suspend", "inline", "noinline", "crossinline",
    "reified", "operator", "infix", "tailrec",
    "vararg", "lateinit", "by", "dynamic",
    "external", "annotation", "expect", "actual",
    "it", "where",
    NULL
};

static const char *kotlin_types[] = {
    "Int", "Long", "Short", "Byte",
    "Double", "Float", "Char", "Boolean", "String",
    "Unit", "Nothing", "Any",
    "Array", "List", "MutableList",
    "Set", "MutableSet", "Map", "MutableMap",
    "Sequence", "Pair", "Triple",
    "IntArray", "LongArray", "ShortArray", "ByteArray",
    "DoubleArray", "FloatArray", "CharArray", "BooleanArray",
    "Regex",
    "Throwable", "Exception", "RuntimeException",
    "IllegalArgumentException", "IllegalStateException",
    "NullPointerException", "IOException",
    "StringBuilder",
    "Comparable", "Iterator", "Iterable",
    "Collection", "MutableCollection",
    "Annotation", "Target", "Retention",
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
 * Go
 * ────────────────────────────────────────────── */
static const char *go_keywords[] = {
    "break", "case", "chan", "const", "continue",
    "default", "defer", "else", "fallthrough", "for",
    "func", "go", "goto", "if", "import",
    "interface", "map", "package", "range", "return",
    "select", "struct", "switch", "type", "var",
    NULL
};

static const char *go_types[] = {
    /* tipos predeclarados */
    "any", "bool", "byte", "comparable", "complex64",
    "complex128", "error", "float32", "float64",
    "int", "int8", "int16", "int32", "int64",
    "rune", "string", "uint", "uint8", "uint16",
    "uint32", "uint64", "uintptr",
    /* literales y constantes predeclaradas */
    "true", "false", "nil", "iota",
    /* funciones builtin */
    "append", "cap", "clear", "close", "complex",
    "copy", "delete", "imag", "len", "make",
    "max", "min", "new", "panic", "print",
    "println", "real", "recover",
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
 * Properties / INI (clave=valor): las claves, comentarios
 * y secciones se resaltan con un tokenizador especializado,
 * así que no se necesitan palabras clave ni tipos
 * ────────────────────────────────────────────── */
static const char *properties_keywords[] = {
    NULL
};

static const char *properties_types[] = {
    NULL
};

/* ──────────────────────────────────────────────
 * YAML: las claves, comentarios, anclas, tags y escalares
 * en bloque se resaltan con un tokenizador especializado;
 * aquí solo se listan los literales escalares
 * ────────────────────────────────────────────── */
static const char *yaml_keywords[] = {
    "true", "false", "null", "yes", "no", "on", "off",
    NULL
};

static const char *yaml_types[] = {
    NULL
};

/* ──────────────────────────────────────────────
 * Gitignore: los patrones, la negación (!) y los comentarios
 * se resaltan con un tokenizador especializado,
 * así que no se necesitan palabras clave ni tipos
 * ────────────────────────────────────────────── */
static const char *gitignore_keywords[] = {
    NULL
};

static const char *gitignore_types[] = {
    NULL
};

/* ──────────────────────────────────────────────
 * XML / HTML (markup simple: tags, comentarios, entidades)
 * ────────────────────────────────────────────── */
static const char *xml_keywords[] = {
    NULL
};

static const char *xml_types[] = {
    NULL
};

/* ──────────────────────────────────────────────
 * SQL
 * ────────────────────────────────────────────── */
static const char *sql_keywords[] = {
    /* DML */
    "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES",
    "UPDATE", "SET", "DELETE", "MERGE", "REPLACE",
    /* DDL */
    "CREATE", "ALTER", "DROP", "TRUNCATE", "RENAME",
    "TABLE", "INDEX", "VIEW", "TRIGGER", "PROCEDURE",
    "FUNCTION", "DATABASE", "SCHEMA", "COLUMN",
    "ADD", "MODIFY", "TYPE", "OWNER",
    /* DCL / TCL */
    "GRANT", "REVOKE", "COMMIT", "ROLLBACK", "SAVEPOINT",
    "BEGIN", "TRANSACTION", "SAVEPOINT",
    /* cláusulas */
    "GROUP", "BY", "HAVING", "ORDER", "ASC", "DESC",
    "LIMIT", "OFFSET", "FETCH", "NEXT", "ROWS", "ONLY",
    "UNION", "ALL", "INTERSECT", "EXCEPT", "MINUS",
    "DISTINCT", "DISTINCTROW",
    /* joins */
    "JOIN", "INNER", "LEFT", "RIGHT", "FULL", "OUTER",
    "CROSS", "NATURAL", "USING", "ON", "LATERAL",
    /* lógica */
    "AND", "OR", "NOT", "IN", "BETWEEN", "LIKE", "ILIKE",
    "IS", "NULL", "EXISTS", "ANY", "SOME",
    "TRUE", "FALSE", "UNKNOWN",
    /* CASE */
    "CASE", "WHEN", "THEN", "ELSE", "END",
    /* constraints */
    "PRIMARY", "KEY", "FOREIGN", "REFERENCES",
    "UNIQUE", "CHECK", "CONSTRAINT", "DEFAULT",
    "CASCADE", "RESTRICT", "NO", "ACTION",
    "NULLS", "FIRST", "LAST",
    /* CTE / window */
    "WITH", "RECURSIVE", "AS",
    "OVER", "PARTITION", "WINDOW", "RANGE", "ROWS",
    "UNBOUNDED", "PRECEDING", "FOLLOWING", "CURRENT", "ROW",
    /* otros */
    "EXPLAIN", "ANALYZE", "ANALYSE", "VACUUM", "REINDEX",
    "TEMPORARY", "TEMP", "MATERIALIZED", "IF",
    "RETURNING", "CONFLICT", "DO", "NOTHING",
    "UPSERT", "ON", "CONFLICT",
    "CUBE", "ROLLUP", "GROUPING", "SETS",
    "FILTER", "ARRAY", "AGGREGATE",
    "ILIKE", "SIMILAR", "REGEXP", "RLIKE",
    "ESCAPE",
    "FOR", "SHARE", "NOWAIT", "SKIP", "LOCKED",
    "ASC", "DESC", "NULLS",
    "CALL", "EXEC", "EXECUTE",
    "USE", "SHOW", "DESCRIBE", "DESC",
    "REPLACE",
    "ENGINE", "AUTO_INCREMENT", "CHARSET", "COLLATE",
    "SEQUENCE", "GENERATED", "ALWAYS", "IDENTITY",
    "CACHE", "CYCLE", "INCREMENT",
    NULL
};

static const char *sql_types[] = {
    "INT", "INTEGER", "SMALLINT", "BIGINT", "TINYINT",
    "MEDIUMINT", "INT2", "INT4", "INT8",
    "NUMERIC", "DECIMAL", "DEC", "REAL", "FLOAT",
    "DOUBLE", "PRECISION", "FLOAT4", "FLOAT8",
    "VARCHAR", "CHAR", "CHARACTER", "TEXT", "CLOB",
    "TINYTEXT", "MEDIUMTEXT", "LONGTEXT",
    "NVARCHAR", "NCHAR", "NVARCHAR2",
    "BOOLEAN", "BOOL",
    "DATE", "TIME", "TIMESTAMP", "DATETIME", "INTERVAL",
    "TIMETZ", "TIMESTAMPTZ", "DATE",
    "BLOB", "TINYBLOB", "MEDIUMBLOB", "LONGBLOB",
    "BINARY", "VARBINARY", "BYTEA", "RAW",
    "JSON", "JSONB",
    "UUID",
    "SERIAL", "BIGSERIAL", "SMALLSERIAL",
    "SERIAL2", "SERIAL4", "SERIAL8",
    "ARRAY", "ENUM", "SET",
    "MONEY", "BIT",
    "GEOMETRY", "GEOGRAPHY",
    "POINT", "LINESTRING", "POLYGON",
    "XML",
    "UNIQUEIDENTIFIER", "ROWVERSION",
    "SQL_VARIANT", "NTEXT", "IMAGE",
    "OID", "REGCLASS", "REGPROC", "REGNAMESPACE",
    "NAME", "BPCHAR",
    "CITEXT", "INET", "CIDR", "MACADDR",
    "TSVECTOR", "TSQUERY",
    NULL
};

/* ──────────────────────────────────────────────
 * Gherkin (Cucumber): features, escenarios y pasos
 * ────────────────────────────────────────────── */
static const char *gherkin_keywords[] = {
    /* estructuras */
    "Feature", "Rule", "Background",
    "Scenario", "Outline", "Example", "Examples",
    /* pasos */
    "Given", "When", "Then", "And", "But",
    /* español */
    "Característica", "Regla", "Antecedentes",
    "Escenario", "Esquema", "Ejemplos",
    "Dado", "Dada", "Dados", "Dadas",
    "Cuando", "Entonces", "Y", "Pero",
    NULL
};

static const char *gherkin_types[] = {
    NULL
};

/* ──────────────────────────────────────────────
 * tabla de lenguajes soportados
 * ────────────────────────────────────────────── */
static const LangDef languages[] = {
    { "c",          c_keywords,   c_types,          1, 0, 0, 0, 0, 0, 0, 0},
    { "cpp",        cpp_keywords, cpp_types,        1, 0, 0, 0, 0, 0, 0, 0},
    { "c++",        cpp_keywords, cpp_types,        1, 0, 0, 0, 0, 0, 0, 0},
    { "cc",         cpp_keywords, cpp_types,        1, 0, 0, 0, 0, 0, 0, 0},
    { "cxx",        cpp_keywords, cpp_types,        1, 0, 0, 0, 0, 0, 0, 0},
    { "h",          c_keywords,   c_types,          1, 0, 0, 0, 0, 0, 0, 0},
    { "hpp",        cpp_keywords, cpp_types,        1, 0, 0, 0, 0, 0, 0, 0},
    { "java",       java_keywords, java_types,      0, 0, 0, 0, 0, 0, 0, 0},
    { "javascript", js_keywords,  js_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "js",         js_keywords,  js_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "ts",         js_keywords,  js_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "typescript", js_keywords,  js_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "cs",         cs_keywords,  cs_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "csharp",     cs_keywords,  cs_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "c#",         cs_keywords,  cs_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "vb",         vb_keywords,  vb_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "vbnet",      vb_keywords,  vb_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "vb.net",     vb_keywords,  vb_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "visualbasic", vb_keywords, vb_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "json",       json_keywords, json_types,      0, 0, 0, 0, 0, 0, 0, 0},
    { "go",         go_keywords,  go_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "golang",     go_keywords,  go_types,         0, 0, 0, 0, 0, 0, 0, 0},
    { "properties", properties_keywords, properties_types, 0, 0, 0, 0, 0, 1, 0, 0},
    { "props",      properties_keywords, properties_types, 0, 0, 0, 0, 0, 1, 0, 0},
    { "ini",        properties_keywords, properties_types, 0, 0, 0, 0, 0, 1, 0, 0},
    { "yaml",       yaml_keywords, yaml_types,      0, 0, 0, 0, 0, 0, 1, 0},
    { "yml",        yaml_keywords, yaml_types,      0, 0, 0, 0, 0, 0, 1, 0},
    { "python",     py_keywords,  py_types,         0, 1, 0, 0, 0, 0, 0, 0},
    { "py",         py_keywords,  py_types,         0, 1, 0, 0, 0, 0, 0, 0},
    { "xml",        xml_keywords, xml_types,        0, 0, 1, 0, 0, 0, 0, 0},
    { "html",       xml_keywords, xml_types,        0, 0, 1, 0, 0, 0, 0, 0},
    { "htm",        xml_keywords, xml_types,        0, 0, 1, 0, 0, 0, 0, 0},
    { "xhtml",      xml_keywords, xml_types,        0, 0, 1, 0, 0, 0, 0, 0},
    { "svg",        xml_keywords, xml_types,        0, 0, 1, 0, 0, 0, 0, 0},
    { "markup",     xml_keywords, xml_types,        0, 0, 1, 0, 0, 0, 0, 0},
    { "sql",        sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "mysql",      sql_keywords, sql_types,        0, 1, 0, 1, 0, 0, 0, 0},
    { "mariadb",    sql_keywords, sql_types,        0, 1, 0, 1, 0, 0, 0, 0},
    { "pgsql",      sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "postgresql", sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "postgres",   sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "plpgsql",    sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "sqlite",     sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "sqlite3",    sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "mssql",      sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "tsql",       sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "plsql",      sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "oracle",     sql_keywords, sql_types,        0, 0, 0, 1, 0, 0, 0, 0},
    { "kotlin",     kotlin_keywords, kotlin_types,  0, 0, 0, 0, 0, 0, 0, 0},
    { "kt",         kotlin_keywords, kotlin_types,  0, 0, 0, 0, 0, 0, 0, 0},
    { "kts",        kotlin_keywords, kotlin_types,  0, 0, 0, 0, 0, 0, 0, 0},
    { "gherkin",    gherkin_keywords, gherkin_types, 0, 1, 0, 0, 1, 0, 0, 0},
    { "feature",    gherkin_keywords, gherkin_types, 0, 1, 0, 0, 1, 0, 0, 0},
    { "gitignore",  gitignore_keywords, gitignore_types, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    { "git-ignore", gitignore_keywords, gitignore_types, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    { "ignore",     gitignore_keywords, gitignore_types, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    { NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
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
 * helper: comparar word con s, sin distinguir mayúsculas
 * (para .inf/.nan de YAML)
 * ────────────────────────────────────────────── */
static int ci_word_match(const char *s, const char *w) {
    while (*w) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*w))
            return 0;
        s++;
        w++;
    }
    return 1;
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
    st->in_xml_comment   = 0;
    st->in_yaml_block    = 0;
    st->yaml_block_indent = 0;
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

/* ══════════════════════════════════════════════════════════════
 * tokenizador XML/HTML (markup simple)
 * ══════════════════════════════════════════════════════════════ */
static int highlight_xml_line(ParsedLine *line, const char *text,
                               HighlightState *st) {
    int len = (int)strlen(text);
    int i   = 0;
    CBuf buf;
    cb_init(&buf);

    /* ── si venimos de un comentario XML abierto ── */
    if (st->in_xml_comment) {
        const char *end = strstr(text, "-->");
        if (end) {
            int comment_end = (int)(end - text) + 3;
            char *comment = strndup(text, (size_t)comment_end);
            emit_span(line, comment, SPAN_KW_COMMENT);
            free(comment);
            i = comment_end;
            st->in_xml_comment = 0;
        } else {
            emit_span(line, text, SPAN_KW_COMMENT);
            return 0;
        }
    }

    while (i < len) {
        /* ── comentario XML: <!-- ... --> ── */
        if (text[i] == '<' && text[i+1] == '!' &&
            text[i+2] == '-' && text[i+3] == '-') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i += 4;
            const char *end = strstr(text + i, "-->");
            if (end) {
                i = (int)(end - text) + 3;
                char *comment = strndup(text + start, (size_t)(i - start));
                emit_span(line, comment, SPAN_KW_COMMENT);
                free(comment);
            } else {
                emit_span(line, text + start, SPAN_KW_COMMENT);
                st->in_xml_comment = 1;
                return 0;
            }
            continue;
        }

        /* ── etiqueta XML: <...> ── */
        if (text[i] == '<') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            /* buscar el > de cierre respetando comillas */
            int in_quote = 0;
            char quote_char = 0;
            while (i < len) {
                if (in_quote) {
                    if (text[i] == quote_char) in_quote = 0;
                } else {
                    if (text[i] == '"' || text[i] == '\'') {
                        in_quote = 1;
                        quote_char = text[i];
                    } else if (text[i] == '>') {
                        i++;
                        break;
                    }
                }
                i++;
            }
            char *tag = strndup(text + start, (size_t)(i - start));
            emit_span(line, tag, SPAN_KW_KEYWORD);
            free(tag);
            continue;
        }

        /* ── entidad XML: &...; ── */
        if (text[i] == '&') {
            int start = i;
            i++;
            if (i < len && text[i] == '#') {
                i++;
                if (i < len && (text[i] == 'x' || text[i] == 'X'))
                    i++;
                while (i < len && (isalnum((unsigned char)text[i]) ||
                                   text[i] == '_')) i++;
            } else {
                while (i < len && (isalnum((unsigned char)text[i]) ||
                                   text[i] == '_')) i++;
            }
            if (i < len && text[i] == ';') {
                i++;
                cb_flush(&buf, line, SPAN_KW_NORMAL);
                char *ent = strndup(text + start, (size_t)(i - start));
                emit_span(line, ent, SPAN_KW_TYPE);
                free(ent);
                continue;
            }
            /* no es una entidad válida, tratar como normal */
            i = start + 1;
            cb_put(&buf, text[start]);
            continue;
        }

        /* ── texto normal ── */
        cb_put(&buf, text[i++]);
    }

    cb_flush(&buf, line, SPAN_KW_NORMAL);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * tokenizador Properties / INI: clave=valor, comentarios
 * # y ! al inicio de línea, secciones [nombre] típicas de INI
 * ══════════════════════════════════════════════════════════════ */
static int highlight_properties_line(ParsedLine *line, const char *text,
                                     HighlightState *st) {
    int len = (int)strlen(text);
    int i   = 0;
    (void)st;  /* sin estado que persista entre líneas */

    /* ── espacios iniciales → texto normal ── */
    while (i < len && (text[i] == ' ' || text[i] == '\t')) i++;
    if (i > 0) {
        char *ws = strndup(text, (size_t)i);
        emit_span(line, ws, SPAN_KW_NORMAL);
        free(ws);
    }

    /* ── comentario: # o ! como primer carácter no-espacio ── */
    if (i < len && (text[i] == '#' || text[i] == '!')) {
        emit_span(line, text + i, SPAN_KW_COMMENT);
        return 0;
    }

    /* ── sección INI: [nombre] ── */
    if (i < len && text[i] == '[') {
        int start = i;
        i++;
        while (i < len && text[i] != ']') i++;
        if (i < len) i++;  /* ']' de cierre */
        char *sec = strndup(text + start, (size_t)(i - start));
        emit_span(line, sec, SPAN_KW_KEYWORD);
        free(sec);
        /* el resto de la línea tras ] */
        if (i < len) emit_span(line, text + i, SPAN_KW_NORMAL);
        return 0;
    }

    /* ── clave: alfanuméricos, punto, guion, guion bajo ── */
    if (i < len && (isalnum((unsigned char)text[i]) ||
                    text[i] == '.' || text[i] == '-' || text[i] == '_')) {
        int start = i;
        while (i < len && (isalnum((unsigned char)text[i]) ||
                           text[i] == '.' || text[i] == '-' ||
                           text[i] == '_' || (unsigned char)text[i] >= 0x80))
            i++;
        char *key = strndup(text + start, (size_t)(i - start));
        emit_span(line, key, SPAN_KW_KEYWORD);
        free(key);
    }

    /* ── separador (= o :) y valor → texto normal ── */
    if (i < len) emit_span(line, text + i, SPAN_KW_NORMAL);

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * tokenizador YAML: claves (clave: valor), comentarios #,
 * strings, números (incluye fechas 2024-01-15), literales
 * (true/false/null/yes/no/on/off/~), anclas &x, alias *x,
 * tags !t, marcadores --- / ... y escalares en bloque | y >
 * ══════════════════════════════════════════════════════════════ */
static int highlight_yaml_line(ParsedLine *line, const char *text,
                               HighlightState *st) {
    int len = (int)strlen(text);
    int i   = 0;
    CBuf buf;
    cb_init(&buf);

    /* indentación de la línea (para los escalares en bloque) */
    int indent = 0;
    while (indent < len && (text[indent] == ' ' || text[indent] == '\t'))
        indent++;

    /* ── dentro de un escalar en bloque (| o >) ── */
    if (st->in_yaml_block) {
        if (indent == len) {
            /* línea en blanco: no cierra el bloque */
            emit_span(line, text, SPAN_KW_STRING);
            return 0;
        }
        if (indent > st->yaml_block_indent) {
            /* contenido del bloque: todo es string */
            emit_span(line, text, SPAN_KW_STRING);
            return 0;
        }
        /* la indentación ya no continúa el bloque: cerrar */
        st->in_yaml_block = 0;
    }

    int at_line_start = 1;  /* solo se han consumido espacios */

    while (i < len) {
        /* ── espacios y tabs → texto normal ── */
        if (text[i] == ' ' || text[i] == '\t') {
            cb_put(&buf, text[i++]);
            continue;
        }

        /* ── marcador de documento: --- o ... al inicio ── */
        if (at_line_start &&
            text[i] == '-' && text[i + 1] == '-' && text[i + 2] == '-' &&
            (i + 3 >= len || text[i + 3] == ' ' || text[i + 3] == '\t')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, "---", SPAN_KW_KEYWORD);
            i += 3;
            at_line_start = 0;
            continue;
        }
        if (at_line_start &&
            text[i] == '.' && text[i + 1] == '.' && text[i + 2] == '.' &&
            (i + 3 >= len || text[i + 3] == ' ' || text[i + 3] == '\t')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, "...", SPAN_KW_KEYWORD);
            i += 3;
            at_line_start = 0;
            continue;
        }

        /* ── ítem de lista: "- " o "-clave" al inicio de línea ── */
        if (at_line_start && text[i] == '-') {
            char nx = text[i + 1];
            if (nx == ' ' || nx == '\t' || nx == '\0' ||
                isalpha((unsigned char)nx) || nx == '_' ||
                nx == '"' || nx == '\'') {
                cb_flush(&buf, line, SPAN_KW_NORMAL);
                emit_span(line, "-", SPAN_KW_KEYWORD);
                i++;
                at_line_start = 0;
                continue;
            }
        }

        /* ── número: entero, negativo, 0x/0o/0b, float,
               .inf/.nan y fechas 2024-01-15 ── */
        if (isdigit((unsigned char)text[i]) ||
            (text[i] == '-' && isdigit((unsigned char)text[i + 1])) ||
            (text[i] == '.' &&
             ((isdigit((unsigned char)text[i + 1])) ||
              ci_word_match(text + i + 1, "inf") ||
              ci_word_match(text + i + 1, "nan")))) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;

            /* .inf / .nan */
            if (text[i] == '.' && !isdigit((unsigned char)text[i + 1])) {
                i += 4;
                goto yaml_num_emit;
            }

            if (text[i] == '-') i++;  /* signo negativo */

            /* prefijo de base: 0x, 0o, 0b */
            if (text[i] == '0' && i + 1 < len) {
                char nx = (char)tolower((unsigned char)text[i + 1]);
                if (nx == 'x' || nx == 'o' || nx == 'b') {
                    i += 2;
                    while (i < len &&
                           ((nx == 'x' && is_hex_digit(text[i])) ||
                            (nx == 'o' && is_octal_digit(text[i])) ||
                            (nx == 'b' && is_bin_digit(text[i])) ||
                            text[i] == '_'))
                        i++;
                    goto yaml_num_emit;
                }
            }

            /* parte entera */
            int digits = 0;
            while (i < len && (isdigit((unsigned char)text[i]) ||
                               text[i] == '_')) {
                if (text[i] != '_') digits++;
                i++;
            }

            /* parte fraccionaria */
            if (i < len && text[i] == '.') {
                i++;
                while (i < len && (isdigit((unsigned char)text[i]) ||
                                   text[i] == '_')) i++;
            }

            /* exponente */
            if (i < len && (text[i] == 'e' || text[i] == 'E')) {
                i++;
                if (i < len && (text[i] == '+' || text[i] == '-')) i++;
                while (i < len && (isdigit((unsigned char)text[i]) ||
                                   text[i] == '_')) i++;
            }

            /* fecha/hora YAML: 2024-01-15 o 2024-01-15T10:30:00 */
            if (digits == 4 && i - start == 4 &&
                i < len && text[i] == '-' &&
                isdigit((unsigned char)text[i + 1]) &&
                isdigit((unsigned char)text[i + 2])) {
                i += 3;  /* -dd */
                if (i < len && text[i] == '-' &&
                    isdigit((unsigned char)text[i + 1]) &&
                    isdigit((unsigned char)text[i + 2])) {
                    i += 3;  /* -dd */
                    /* parte horaria: T o espacio + hh:mm[:ss(.frac)] */
                    if ((text[i] == 'T' || text[i] == 't' ||
                         text[i] == ' ') &&
                        isdigit((unsigned char)text[i + 1]) &&
                        isdigit((unsigned char)text[i + 2]) &&
                        text[i + 3] == ':') {
                        i += 4;  /* hh: */
                        while (i < len &&
                               (isdigit((unsigned char)text[i]) ||
                                text[i] == ':' || text[i] == '.'))
                            i++;
                        if (i < len &&
                            (text[i] == 'Z' || text[i] == 'z')) i++;
                    }
                }
            }

        yaml_num_emit:
            char *num = strndup(text + start, (size_t)(i - start));
            emit_span(line, num, SPAN_KW_NUMBER);
            free(num);
            at_line_start = 0;
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
            at_line_start = 0;
            continue;
        }

        /* ── string con comillas simples: '...' ('' = comilla literal) ── */
        if (text[i] == '\'') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len) {
                if (text[i] != '\'') { i++; continue; }
                if (text[i + 1] == '\'') { i += 2; continue; }
                i++;  /* comilla de cierre */
                break;
            }
            char *s = strndup(text + start, (size_t)(i - start));
            emit_span(line, s, SPAN_KW_STRING);
            free(s);
            at_line_start = 0;
            continue;
        }

        /* ── comentario: # al inicio o precedido de espacio
               (los strings ya se consumen completos) ── */
        if (text[i] == '#' &&
            (i == 0 || text[i - 1] == ' ' || text[i - 1] == '\t')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, text + i, SPAN_KW_COMMENT);
            return 0;  /* resto de la línea es comentario */
        }

        /* ── ancla: &nombre ── */
        if (text[i] == '&' &&
            (isalnum((unsigned char)text[i + 1]) || text[i + 1] == '_')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && (isalnum((unsigned char)text[i]) ||
                               text[i] == '_' || text[i] == '-'))
                i++;
            char *a = strndup(text + start, (size_t)(i - start));
            emit_span(line, a, SPAN_KW_TYPE);
            free(a);
            at_line_start = 0;
            continue;
        }

        /* ── tag: !tag, !!str, !<...> ── */
        if (text[i] == '!') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            if (i < len && text[i] == '<') {
                while (i < len && text[i] != '>') i++;
                if (i < len) i++;  /* '>' de cierre */
            } else {
                while (i < len && (isalnum((unsigned char)text[i]) ||
                                   text[i] == '_' || text[i] == '-' ||
                                   text[i] == '!'))
                    i++;
            }
            char *t = strndup(text + start, (size_t)(i - start));
            emit_span(line, t, SPAN_KW_TYPE);
            free(t);
            at_line_start = 0;
            continue;
        }

        /* ── alias: *nombre (tras espacio, : , [ o {) ── */
        if (text[i] == '*' &&
            (isalnum((unsigned char)text[i + 1]) || text[i + 1] == '_') &&
            (i == 0 || text[i - 1] == ' ' || text[i - 1] == '\t' ||
             text[i - 1] == ':' || text[i - 1] == ',' ||
             text[i - 1] == '[' || text[i - 1] == '{')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && (isalnum((unsigned char)text[i]) ||
                               text[i] == '_' || text[i] == '-'))
                i++;
            char *a = strndup(text + start, (size_t)(i - start));
            emit_span(line, a, SPAN_KW_TYPE);
            free(a);
            at_line_start = 0;
            continue;
        }

        /* ── indicador de escalar en bloque: | o > ── */
        if (text[i] == '|' || text[i] == '>') {
            /* el char no-espacio anterior debe ser : o - */
            int k = i - 1;
            while (k >= 0 && (text[k] == ' ' || text[k] == '\t')) k--;
            int ctx = (k < 0 || text[k] == ':' || text[k] == '-');
            if (ctx) {
                /* tras el indicador solo van +- , dígitos 1-9 y
                   opcionalmente un comentario */
                int j = i + 1;
                while (j < len && (text[j] == '+' || text[j] == '-' ||
                                   (text[j] >= '1' && text[j] <= '9')))
                    j++;
                while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
                if (j >= len || text[j] == '#') {
                    cb_flush(&buf, line, SPAN_KW_NORMAL);
                    int m = i + 1;
                    while (m < len && (text[m] == '+' || text[m] == '-' ||
                                       (text[m] >= '1' && text[m] <= '9')))
                        m++;
                    char *ind = strndup(text + i, (size_t)(m - i));
                    emit_span(line, ind, SPAN_KW_KEYWORD);
                    free(ind);
                    st->in_yaml_block = 1;
                    st->yaml_block_indent = indent;
                    at_line_start = 0;
                    if (m < j) {  /* espacios antes del comentario */
                        char *ws = strndup(text + m, (size_t)(j - m));
                        emit_span(line, ws, SPAN_KW_NORMAL);
                        free(ws);
                    }
                    if (j < len)
                        emit_span(line, text + j, SPAN_KW_COMMENT);
                    return 0;
                }
            }
        }

        /* ── literal ~ (null) ── */
        if (text[i] == '~') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, "~", SPAN_KW_KEYWORD);
            i++;
            at_line_start = 0;
            continue;
        }

        /* ── identificador: clave si le sigue ":" + espacio/EOL,
               o literal true/false/null/yes/no/on/off ── */
        if (isalpha((unsigned char)text[i]) || text[i] == '_') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            while (i < len &&
                   (isalnum((unsigned char)text[i]) || text[i] == '_' ||
                    text[i] == '-' || text[i] == '.' ||
                    (unsigned char)text[i] >= 0x80))
                i++;
            char *word = strndup(text + start, (size_t)(i - start));

            /* "palabra:" seguida de espacio o fin de línea → clave */
            int j = i;
            while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
            int is_key = (j < len && text[j] == ':' &&
                          (j + 1 >= len || text[j + 1] == ' ' ||
                           text[j + 1] == '\t'));

            if (is_key || str_in_array(word, yaml_keywords))
                emit_span(line, word, SPAN_KW_KEYWORD);
            else
                emit_span(line, word, SPAN_KW_NORMAL);

            free(word);
            at_line_start = 0;
            continue;
        }

        /* ── cualquier otro carácter: : , [ ] { } etc. ── */
        cb_put(&buf, text[i++]);
        at_line_start = 0;
    }

    cb_flush(&buf, line, SPAN_KW_NORMAL);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * tokenizador Gitignore: comentarios #, negación ! al inicio,
 * patrones con comodines (* ** ?) y / final de directorio
 * ══════════════════════════════════════════════════════════════ */
static int highlight_gitignore_line(ParsedLine *line, const char *text,
                                    HighlightState *st) {
    int len = (int)strlen(text);
    int i   = 0;
    (void)st;  /* sin estado que persista entre líneas */
    CBuf buf;
    cb_init(&buf);

    /* ── espacios iniciales → texto normal ── */
    while (i < len && (text[i] == ' ' || text[i] == '\t'))
        cb_put(&buf, text[i++]);

    /* ── comentario: # como primer carácter no-espacio y sin
       escaparse (\# es un patrón literal) ── */
    if (i < len && text[i] == '#' && (i == 0 || text[i - 1] != '\\')) {
        cb_flush(&buf, line, SPAN_KW_NORMAL);
        emit_span(line, text + i, SPAN_KW_COMMENT);
        return 0;
    }

    /* ── negación: ! al inicio del patrón ── */
    if (i < len && text[i] == '!') {
        cb_flush(&buf, line, SPAN_KW_NORMAL);
        emit_span(line, "!", SPAN_KW_KEYWORD);
        i++;
    }

    /* ── patrón: texto normal, comodines * ** ? y / final de
       directorio como marcadores ── */
    while (i < len) {
        if (text[i] == '*') {
            cb_flush(&buf, line, SPAN_KW_STRING);
            if (text[i + 1] == '*') {
                emit_span(line, "**", SPAN_KW_NUMBER);
                i += 2;
            } else {
                emit_span(line, "*", SPAN_KW_NUMBER);
                i++;
            }
            continue;
        }
        if (text[i] == '?') {
            cb_flush(&buf, line, SPAN_KW_STRING);
            emit_span(line, "?", SPAN_KW_NUMBER);
            i++;
            continue;
        }
        /* / al final de línea → patrón de directorio */
        if (text[i] == '/' && i + 1 >= len) {
            cb_flush(&buf, line, SPAN_KW_STRING);
            emit_span(line, "/", SPAN_KW_TYPE);
            i++;
            continue;
        }
        cb_put(&buf, text[i++]);
    }

    cb_flush(&buf, line, SPAN_KW_STRING);
    return 0;
}

int highlight_line(ParsedLine *line, const char *text,
                   const char *lang, HighlightState *st) {
    const LangDef *ld = find_lang(lang);
    if (!ld || !line) return -1;

    /* XML/HTML: usar tokenizador especializado */
    if (ld->is_xml)
        return highlight_xml_line(line, text, st);

    /* Properties/INI: tokenizador especializado (clave=valor) */
    if (ld->is_properties)
        return highlight_properties_line(line, text, st);

    /* YAML: tokenizador especializado (claves, comentarios...) */
    if (ld->is_yaml)
        return highlight_yaml_line(line, text, st);

    /* Gitignore: tokenizador especializado (patrones, ! y #) */
    if (ld->is_gitignore)
        return highlight_gitignore_line(line, text, st);

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

        /* ── comentario de línea: -- (SQL) ── */
        if (text[i] == '-' && text[i + 1] == '-' && ld->has_dash_comment) {
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

        /* ── tag de Gherkin: @nombre (solo al inicio de línea, tras
           espacios: evita resaltar emails dentro de pasos) ── */
        if (ld->is_gherkin && text[i] == '@' &&
            (i == 0 || text[i - 1] == ' ' || text[i - 1] == '\t')) {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && (isalnum((unsigned char)text[i]) ||
                               text[i] == '_' || text[i] == '-'))
                i++;
            char *tag = strndup(text + start, (size_t)(i - start));
            emit_span(line, tag, SPAN_KW_PREPROC);
            free(tag);
            continue;
        }

        /* ── placeholder de Gherkin: <var> ── */
        if (ld->is_gherkin && text[i] == '<') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            i++;
            while (i < len && text[i] != '>') i++;
            if (i < len) i++;  /* '>' de cierre */
            char *ph = strndup(text + start, (size_t)(i - start));
            emit_span(line, ph, SPAN_KW_TYPE);
            free(ph);
            continue;
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

        /* ── template literal JS/TS, raw string Go y
           backtick identifiers MySQL ── */
        if (text[i] == '`' &&
            (strcmp(ld->name, "javascript") == 0 ||
             strcmp(ld->name, "js") == 0 ||
             strcmp(ld->name, "ts") == 0 ||
             strcmp(ld->name, "typescript") == 0 ||
             strcmp(ld->name, "mysql") == 0 ||
             strcmp(ld->name, "mariadb") == 0 ||
             strcmp(ld->name, "go") == 0 ||
             strcmp(ld->name, "golang") == 0)) {
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

            /* sufijo Go: i (literales imaginarios, p. ej. 1i) */
            if (i < len && text[i] == 'i' &&
                (strcmp(ld->name, "go") == 0 ||
                 strcmp(ld->name, "golang") == 0)) i++;

            char *num = strndup(text + start, (size_t)(i - start));
            emit_span(line, num, SPAN_KW_NUMBER);
            free(num);
            continue;
        }

        /* ── identificador / palabra clave ── */
        if (isalpha((unsigned char)text[i]) || text[i] == '_') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            int start = i;
            /* bytes >= 0x80: permitir UTF-8 dentro de la palabra
               (p. ej. keywords de Gherkin acentuadas como "Característica") */
            while (i < len &&
                   (isalnum((unsigned char)text[i]) || text[i] == '_' ||
                    (unsigned char)text[i] >= 0x80))
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

        /* ── marcador de paso de Gherkin: * ── */
        if (ld->is_gherkin && text[i] == '*') {
            cb_flush(&buf, line, SPAN_KW_NORMAL);
            emit_span(line, "*", SPAN_KW_KEYWORD);
            i++;
            continue;
        }

        /* ── cualquier otro carácter: operadores, puntuación, etc. ── */
        cb_put(&buf, text[i++]);
    }

    cb_flush(&buf, line, SPAN_KW_NORMAL);
    return 0;
}
