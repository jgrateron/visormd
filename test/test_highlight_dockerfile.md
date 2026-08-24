# Dockerfile Highlight Test

Instructions, comments, flags, and multi-line bodies.

```dockerfile
# syntax=docker/dockerfile:1
FROM node:20-alpine AS builder
WORKDIR /app
COPY --from=builder /usr/bin/foo /usr/bin/foo
RUN --mount=type=secret,id=api_key \
    export OPENAI_API_KEY=$(cat /run/secrets/api_key) && \
    ./setup.sh
ENV NODE_ENV=production PORT=8080
EXPOSE 8080
LABEL maintainer="devops@example.com" version="1.0"
HEALTHCHECK --interval=30s CMD curl -f http://localhost/ || exit 1
USER app
ARG VERSION=1.0
CMD ["node", "server.js"]
ENTRYPOINT ["docker-entrypoint.sh"]
ONBUILD RUN npm install
SHELL ["/bin/bash", "-c"]
run --mount=type=cache,target=/root/.cache go build
# comentario simple
   # comentario con indentación
FROM scratch
```
