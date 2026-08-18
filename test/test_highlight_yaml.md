# YAML Highlight Test

Comments (`#`), keys (`key: value`), strings, numbers, literals (`true`/`false`/`null`), anchors (`&x`), aliases (`*x`), tags (`!t`), document markers (`---`/`...`), and block scalars (`|` and `>`).

```yaml
# docker-compose.yml de ejemplo
version: "3.9"

services:
  web:
    image: nginx:1.25-alpine
    ports:
      - "8080:80"
    environment:
      - DEBUG=false
      - APP_URL=http://localhost:8080
    restart: always
    volumes:
      - ./app:/usr/share/nginx/html

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: admin
      POSTGRES_PASSWORD: 'secre''to'
      POSTGRES_DB: appdb
    ports:
      - 5432:5432
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U admin"]
      interval: 10s
      timeout: 5s
      retries: 3
```

```yaml
# deploy.yml de ejemplo: anclas, alias, tags y escalares
defaults: &defaults
  adapter: postgres
  pool: 5
  timeout: 5000

production:
  <<: *defaults
  database: app_prod
  retry: yes

development:
  <<: *defaults
  database: app_dev
  verbose: true
  retries: 0
  empty: null
  also_empty: ~

build:
  type: !!str 123
  custom: !mytag valor
  raw: !<tag:yaml.org,2002:int> "42"
  numbers: [1, -2, 3.14, 1.5e3, 0xFF, 0o755, 0b1010, 1_000_000, .inf, .nan]
  dates: [2024-01-15, 2024-01-15T10:30:00, 2024-01-15 22:15:05Z]
  flags: [on, off, no, yes]
```

```yaml
# bloques y marcadores de documento
---
title: Manual de usuario
lang: es
sections:
  - name: Introducción
    pages: 12
  - name: Instalación
    pages: 3
  - name: Preguntas frecuentes
    pages: 7
---

resumen: |
  Esta es una descripción
  larga que ocupa
  varias líneas.

  Con párrafos y líneas en blanco.

notas: |-
  Línea uno
  línea dos

plegado: >
  Este texto plegado
  se une en una línea.

listas:
  - primero
  - segundo
    - anidado.a
    - anidado.b
  - tercero

claves:
  simple: valor
  con.punto: ok
  con-guion: ok
  valor_plano: abc#def
  parecido_a_clave:value
  url: https://example.com/ruta

...
```

```yaml
# comentarios en contexto
clave: valor # comentario tras valor
# comentario de línea completa
   # comentario indentado
texto: "con # almohadilla dentro"
texto2: 'y # otra dentro'
clave3: valor#sin espacio no es comentario
```
