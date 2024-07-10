OUT_NAME=boomer2

CXX=clang++
STD=-std=c++2b
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-command-line-argument -Wno-missing-field-initializers -Wno-gnu-zero-variadic-macro-arguments -Wno-c99-extensions
SANITIZERS=-fdebug-macro -fsanitize=address -fstack-protector -fstack-protector-strong -fstack-protector-all -Rpass=inline -Rpass=unroll -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
LIBS=-lX11 -lraylib -lxcb
CXXCOMMONFLAGS=-DXCB_SCREENSHOT -fopenmp=libomp -flto -g
CXXFLAGS=$(WARNINGS) $(CXXCOMMONFLAGS) -march=native -Ofast
CMD=$(CXX) $(STD) $(CXXFLAGS) $(LIBS)

OBJ_PREFIX=objs

OUT_DIR=./output
OUT_POSTFIX=release
OUT=$(OUT_DIR)/$(OUT_NAME)-$(OUT_POSTFIX)

all: exe

run: exe
	$(OUT)

install: exe
	cp $(OUT) ~/.local/bin/$(OUT_NAME)

debug: export CXXFLAGS=$(WARNINGS) $(SANITIZERS) $(CXXCOMMONFLAGS) -fno-omit-frame-pointer -g -fopenmp=libomp -DDEBUG
debug: export OUT_POSTFIX=debug
debug: export ASAN_OPTIONS=detect_leaks=1
debug: export LSAN_OPTIONS=suppressions=address-sanitizer-suppress, print_suppressions=0, fast_unwind_on_malloc=0
debug: cleanup exe
	rm -rf objs/*
	-$(OUT)
	feh /tmp/__out_image.png

bench: export CXXFLAGS=$(WARNINGS) $(CXXCOMMONFLAGS) -march=native -Ofast -DBENCH
bench: cleanup exe
	mkdir -p bench
	hyperfine --warmup 1 --export-orgmode bench/`date --iso-8601=seconds | sed 's/:/_/g'`.org $(OUT)

cleanup:
	rm -rf ./$(OBJ_PREFIX)/*

exe: $(OBJECTS) Makefile
	$(CXX) $(STD) $(CXXFLAGS) -c src/platform.cpp -o $(OBJ_PREFIX)/platform.o
	$(CXX) $(STD) $(CXXFLAGS) -c src/main.cpp -o $(OBJ_PREFIX)/main.o
	$(CMD) $(CXXFLAGS) objs/main.o objs/platform.o -o $(OUT)
