# JavaScript Highlight Test

ES6+ features: classes, arrow functions, template literals, destructuring.

```javascript
import fs from 'node:fs/promises';

class ConfigLoader {
    #cache = new Map();
    static #instance = null;

    constructor(basePath = '.') {
        this.basePath = basePath;
    }

    async load(name) {
        if (this.#cache.has(name)) {
            return this.#cache.get(name);
        }

        const path = `${this.basePath}/${name}.json`;
        const raw = await fs.readFile(path, 'utf-8');
        const config = JSON.parse(raw);
        this.#cache.set(name, config);
        return config;
    }

    async loadAll(...names) {
        const results = await Promise.all(
            names.map(n => this.load(n))
        );
        return Object.fromEntries(
            names.map((n, i) => [n, results[i]])
        );
    }
}

function processItems(items, options = {}) {
    const {
        filter = () => true,
        transform = (x) => x,
        limit = Infinity,
    } = options;

    const result = items
        .filter(item => item.active && filter(item))
        .slice(0, limit)
        .map(({ id, name, tags }) => ({
            id,
            name: name.toUpperCase(),
            tagCount: tags?.length ?? 0,
        }));

    console.log(`Processed ${result.length} items`);
    return result;
}

// Numbers
const intVal = 42;
const hexVal = 0xFF;
const binVal = 0b1010;
const octVal = 0o755;
const bigVal = 9007199254740991n;
const floatVal = 3.14159;
const sciVal = 1.5e10;

// Strings
const single = 'Hello';
const double = "World";
const template = `Count: ${intVal}`;
const regex = /^[a-z]+$/i;

// Keywords
if (active && count > 0) {
    for (const item of items) {
        if (typeof item === 'object' && item !== null) {
            debugger;
        }
    }
}

export { ConfigLoader, processItems };
```
