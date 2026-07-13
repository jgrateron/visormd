# Python Highlight Test

Python 3.10+: classes, async/await, type hints, match/case, decorators.

```python
import os
from dataclasses import dataclass, field
from typing import Optional, List
from pathlib import Path


@dataclass
class ConfigEntry:
    key: str
    value: str
    source: Optional[str] = None


class ConfigLoader:
    """Carga configuración desde archivos .env."""

    def __init__(self, base_path: str = "."):
        self.base_path = Path(base_path)
        self._cache: dict[str, ConfigEntry] = {}

    async def load(self, name: str) -> dict[str, str]:
        if name in self._cache:
            return self._cache[name]

        path = self.base_path / f"{name}.env"
        if not path.exists():
            raise FileNotFoundError(f"Config not found: {path}")

        content = await self._read_file(path)
        result = self._parse(content)
        self._cache[name] = result
        return result

    async def _read_file(self, path: Path) -> str:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()

    def _parse(self, content: str) -> dict[str, str]:
        result = {}
        for line in content.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                key, _, value = line.partition("=")
                result[key.strip()] = value.strip().strip('"')
        return result


def process_items(items: list[dict], options: dict | None = None):
    """Procesa una lista de items con transformaciones."""
    if options is None:
        options = {}

    limit = options.get("limit", 100)
    active_only = options.get("active_only", True)

    result = []
    for item in items:
        if active_only and not item.get("active", False):
            continue
        if len(result) >= limit:
            break

        match item.get("type"):
            case "user":
                result.append(_transform_user(item))
            case "admin":
                result.append(_transform_admin(item))
            case _:
                result.append(item)

    return result


def _transform_user(item: dict) -> dict:
    return {
        "id": item["id"],
        "name": item["name"].upper(),
        "tags": item.get("tags", []),
    }


# Numeros con underscore (PEP 515)
millon = 1_000_000
hex_color = 0xFF_FF_FF
bin_flags = 0b1010_0101
oct_perm = 0o755

# Numeros complejos
impedance = 3 + 5j
omega = 1.0e-6j

# Numeros cientificos
avogadro = 6.02214076e23
planck = 6.626e-34

# Strings y f-strings
name = "Mundo"
greeting = f"Hola {name}!"
path = r"C:\Users\default\docs"

# Docstring multinlinea
def funcion_con_docstring():
    """Esta es una documentación
    que ocupa varias líneas
    con detalles."""
    pass

# Keywords
if __name__ == "__main__":
    try:
        for i in range(10):
            if i % 2 == 0:
                print(f"Par: {i}")
            else:
                continue
    except Exception as e:
        raise RuntimeError("fallo") from e
    finally:
        del i

    with open("test.txt", "w") as f:
        f.write("hello")

    # async/await
    async def main():
        loader = ConfigLoader()
        await loader.load("database")

    import asyncio
    asyncio.run(main())
```
