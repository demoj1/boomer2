OUT_NAME=boomer2

CXX=clang++
STD=-std=c++2b
WARNINGS=-Wno-unused-command-line-argument -Wall -Wextra -Wpedantic
SANITIZERS=-fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
# SANITIZERS=-fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
LIBS=-lX11 -lraylib
CXXFLAGS=$(WARNINGS) -march=native -O3
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

debug: CXXFLAGS=$(WARNINGS) -g $(SANITIZERS) -DDEBUG
debug: OUT_POSTFIX=debug
debug: cleanup exe
	rm -rf objs/*
	$(OUT)

cleanup:
	rm -rf ./$(OBJ_PREFIX)/*

exe: $(OBJECTS) Makefile
	$(CXX) $(STD) $(CXXFLAGS) -c src/screenshot.cpp -o $(OBJ_PREFIX)/screenshot.o
	$(CXX) $(STD) $(CXXFLAGS) -c src/main.cpp -o $(OBJ_PREFIX)/main.o
	$(CMD) $(CXXFLAGS) objs/main.o objs/screenshot.o -o $(OUT)
