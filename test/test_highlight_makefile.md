# Makefile Highlight Test

Variables, targets, .PHONY, recipes with tabs, variable references and comments.

```makefile
# variables con distintos operadores
APP_NAME=spring-boot-docker-makefile-demo
IMAGE_NAME=spring-boot-docker-makefile-demo
CONTAINER_NAME=spring-boot-docker-makefile-demo-container
VERSION=0.0.1
HOST_PORT=1441
CONTAINER_PORT=1441
FLAGS ?= -O2
CXXFLAGS := -std=c++17
OBJS += main.o extra.o
BUILD_DIR = build # comentario al final del valor
URL_RAW = https://x.com/#frag no es comentario

# directivas
include $(COMMON_DIR)/common.mk
-include local.mk
export PATH := /usr/bin:$(PATH)
ifdef DEBUG
CFLAGS = -g
endif

# target especial .PHONY
.PHONY: help build build-no-cache run up logs stop remove restart clean test health actuator-health shell

build:
	docker build -t $(IMAGE_NAME):$(VERSION) -t $(IMAGE_NAME):latest .

build-no-cache:
	docker build --no-cache -t $(IMAGE_NAME):$(VERSION) .

run:
	-docker rm -f $(CONTAINER_NAME)
	docker run -d --name $(CONTAINER_NAME) -p $(HOST_PORT):$(CONTAINER_PORT) $(IMAGE_NAME):latest

up: build run

logs:
	docker logs -f $(CONTAINER_NAME)

stop:
	-docker stop $(CONTAINER_NAME)

remove:
	-docker rm -f $(CONTAINER_NAME)

restart: stop run

clean: remove
	-docker rmi $(IMAGE_NAME):$(VERSION) $(IMAGE_NAME):latest

test:
	docker run --rm -v "$$(pwd)":/workspace -w /workspace \
		maven:3.9.11-amazoncorretto-25 mvn test

health:
	curl -i http://localhost:$(HOST_PORT)/actuator/health

shell:
	docker exec -it $(CONTAINER_NAME) sh
	@echo "receta silenciosa con @ y $$HOME"

# patrón de regla con %
obj/%.o: src/%.c
	$(CC) -c $< -o $@
```
