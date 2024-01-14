CC = cc

PLATFORM = -DPLATFORM_DESKTOP

# flags for building imFAP
FLAGS = -Wall -Wextra -pedantic -ggdb

SRCS = $(wildcard *.c)
OUTPUT = main

# flags for building raylib locally
CFLAGS = $(PLATFORM) -I$(RAY_PATH)external/glfw/include

RAY_PATH = external/raylib/src/
RAY_SRCS = $(wildcard $(addsuffix *.c, $(RAY_PATH)))
RAY_OBJS = $(RAY_SRCS:c=o)


all: build

run: build
	./$(OUTPUT)

# build from globally installed raylib library
build_from_global: $(SRCS)
	$(CC) -o $(OUTPUT) $(SRCS) $(FLAGS) -lraylib -lm

# build with provided version of raylib
build: $(SRCS) $(RAY_OBJS)
	$(CC) -o $(OUTPUT) $(SRCS) $(RAY_OBJS) $(FLAGS) -lm

clean:
	rm -f $(OUTPUT)
	rm -f $(RAY_OBJS)
