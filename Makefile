OUT_NAME=boomer2

CC=clang
CXX=clang++
STD=-std=c++2b
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-command-line-argument -Wno-missing-field-initializers -Wno-gnu-zero-variadic-macro-arguments -Wno-c99-extensions
SANITIZERS=-fdebug-macro -fsanitize=address -fstack-protector -fstack-protector-strong -fstack-protector-all -Rpass=inline -Rpass=unroll -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
LIBS=-lX11 -lraylib -lxcb
CXXCOMMONFLAGS=-DXCB_SCREENSHOT -flto -g
CXXFLAGS=$(WARNINGS) $(CXXCOMMONFLAGS) -march=native -Ofast
CMD=$(CXX) $(STD) $(CXXFLAGS) $(LIBS)

OBJ_PREFIX=objs

# Generated Wayland protocol glue (xdg-output, needed to read the monitor layout).
GEN_DIR=gen
WL_PROTO_DIR=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
XDG_OUTPUT_XML=$(WL_PROTO_DIR)/unstable/xdg-output/xdg-output-unstable-v1.xml
# Empty for the X11 build, the `wayland` target overrides it.
GEN_OBJS=

OUT_DIR=./output
OUT_POSTFIX=release
OUT=$(OUT_DIR)/$(OUT_NAME)-$(OUT_POSTFIX)

all: exe

run: exe
	$(OUT)

.PHONY: install install-x11 test

# The default desktop is Wayland; install the same backend used by the hotkey.
install: wayland
	install -Dm755 $(OUT_DIR)/$(OUT_NAME)-wayland "$(HOME)/.local/bin/@$(OUT_NAME)"

install-x11: exe
	install -Dm755 $(OUT) "$(HOME)/.local/bin/@$(OUT_NAME)"

test:
	./tests/run.sh

debug: export CXXFLAGS=$(WARNINGS) $(SANITIZERS) $(CXXCOMMONFLAGS) -DLLVM_ENABLE_RUNTIMES=compiler-rt -fno-omit-frame-pointer -g -DDEBUG
debug: export OUT_POSTFIX=debug
debug: export ASAN_OPTIONS=detect_leaks=1
debug: export LSAN_OPTIONS=suppressions=address-sanitizer-suppress, print_suppressions=0, fast_unwind_on_malloc=0
debug: cleanup exe
	rm -rf objs/*
	-$(OUT)

bench: export CXXFLAGS=$(WARNINGS) $(CXXCOMMONFLAGS) -march=native -Ofast -DBENCH
bench: cleanup exe
	mkdir -p bench
	hyperfine --warmup 1 --export-orgmode bench/`date --iso-8601=seconds | sed 's/:/_/g'`.org $(OUT)

# Wayland build: capture the screen via `grim` instead of XCB, copy via wl-copy.
# Requires grim and wl-clipboard at runtime, wayland-scanner and wayland-protocols
# to build. Multi-monitor layout comes from the compositor over xdg-output.
wayland: export CXXFLAGS=$(WARNINGS) -DWAYLAND_SCREENSHOT -I$(GEN_DIR) -flto -g -march=native -Ofast
wayland: export LIBS=-lraylib -lwayland-client
wayland: export OUT_POSTFIX=wayland
wayland: export GEN_OBJS=$(OBJ_PREFIX)/xdg-output-protocol.o
wayland: cleanup wl-proto exe

$(GEN_DIR)/xdg-output-unstable-v1-client-protocol.h: $(XDG_OUTPUT_XML)
	mkdir -p $(GEN_DIR)
	wayland-scanner client-header $< $@

$(GEN_DIR)/xdg-output-unstable-v1-protocol.c: $(XDG_OUTPUT_XML)
	mkdir -p $(GEN_DIR)
	wayland-scanner private-code $< $@

wl-proto: $(GEN_DIR)/xdg-output-unstable-v1-client-protocol.h $(GEN_DIR)/xdg-output-unstable-v1-protocol.c
	mkdir -p $(OBJ_PREFIX)
	$(CC) -O2 -c $(GEN_DIR)/xdg-output-unstable-v1-protocol.c -o $(OBJ_PREFIX)/xdg-output-protocol.o

cleanup:
	rm -rf ./$(OBJ_PREFIX)/*

exe: $(OBJECTS) Makefile
	mkdir -p $(OBJ_PREFIX) $(OUT_DIR)
	$(CXX) $(STD) $(CXXFLAGS) -c src/platform.cpp -o $(OBJ_PREFIX)/platform.o
	$(CXX) $(STD) $(CXXFLAGS) -c src/main.cpp -o $(OBJ_PREFIX)/main.o
	$(CMD) $(CXXFLAGS) objs/main.o objs/platform.o $(GEN_OBJS) -o $(OUT)
