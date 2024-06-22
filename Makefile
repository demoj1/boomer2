OUT_NAME=boomer2

CXX=clang++
STD=-std=c++2b
WARNINGS=-Wno-unused-command-line-argument -Wall -Wextra -Wpedantic
SANITIZERS=-fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
LIBS=-lX11 -lraylib
CXXFLAGS=$(STD) $(WARNINGS) -march=native -Ofast $(LIBS)
CMD=$(CXX) $(CXXFLAGS)

OBJ_PREFIX=objs

OUT_DIR=./output
OUT_POSTFIX=release
OUT=$(OUT_DIR)/$(OUT_NAME)-$(OUT_POSTFIX)

all: exe

run: exe
	$(OUT)

install: exe
	cp $(OUT) ~/.local/bin/$(OUT_NAME)

debug: CXXFLAGS=$(STD) $(WARNINGS) -g $(SANITIZERS) -DDEBUG $(LIBS)
debug: OUT_POSTFIX=debug
debug: cleanup exe
	rm -rf objs/*
	$(OUT)

cleanup:
	rm -rf ./$(OBJ_PREFIX)/*

exe: $(OBJECTS) Makefile
	clang++ -c src/screenshot.cpp -o $(OBJ_PREFIX)/screenshot.o
	clang++ -c src/main.cpp -o $(OBJ_PREFIX)/main.o
	$(CMD) objs/main.o objs/screenshot.o -o $(OUT)