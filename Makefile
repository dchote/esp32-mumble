# ESP32-Mumble build targets

CONFIG ?= generic
PORT ?= /dev/ttyUSB0
ESPHOME_DIR := esphome

.PHONY: build flash all clean

build:
	./scripts/build.sh $(CONFIG)

flash:
	./scripts/flash.sh $(CONFIG) --device $(PORT)

all:
	./scripts/build.sh all

clean:
	rm -rf .esphome/build .pio
