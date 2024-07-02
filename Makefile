OUT_NAME=boomer2

CXX=clang++
STD=-std=c++2b
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-command-line-argument -Wno-missing-field-initializers -Wno-gnu-zero-variadic-macro-arguments -Wno-c99-extensions
# SANITIZERS=-fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
SANITIZERS=-fdebug-macro -fsanitize=address -fstack-protector -fstack-protector-strong -fstack-protector-all -Rpass=inline -Rpass=unroll -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
# SANITIZERS=-fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
LIBS=-lX11 -lraylib
CXXFLAGS=$(WARNINGS) -march=native -flto -Ofast
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

debug: export CXXFLAGS=$(WARNINGS) $(SANITIZERS) -fno-omit-frame-pointer -flto -g -DDEBUG
debug: export OUT_POSTFIX=debug
debug: export ASAN_OPTIONS=detect_leaks=1
debug: export LSAN_OPTIONS=suppressions=address-sanitizer-suppress, print_suppressions=0, fast_unwind_on_malloc=0
debug: cleanup exe
	rm -rf objs/*
	-$(OUT)
	feh /tmp/__out_image.png

cleanup:
	rm -rf ./$(OBJ_PREFIX)/*

exe: $(OBJECTS) Makefile
	$(CXX) $(STD) $(CXXFLAGS) -c src/screenshot.cpp -o $(OBJ_PREFIX)/screenshot.o
	$(CXX) $(STD) $(CXXFLAGS) -c src/main.cpp -o $(OBJ_PREFIX)/main.o
	$(CMD) $(CXXFLAGS) objs/main.o objs/screenshot.o -o $(OUT)
