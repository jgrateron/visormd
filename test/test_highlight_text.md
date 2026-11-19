# Text Highlight Test

Aliases `text`, `txt`, `plaintext`, and `plain` must render as plain text:
no syntax colors and no inline Markdown interpretation.

```text
int main(void) { return 42; } // no es un comentario coloreado
#include <stdio.h>
"cadena" 123 true null
# no es un heading
**no es negrita**
```

```txt
TODO: "string" 3.14 false
```

```plaintext
SELECT * FROM users; -- no es comentario SQL
```

```plain
clave: valor
- no es una lista
```
