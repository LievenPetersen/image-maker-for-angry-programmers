# available targets: Linux, Windows
# (all build on Linux)
TARGET ?= Linux

CC = cc
ifeq ($(TARGET), Windows)
	CC = x86_64-w64-mingw32-gcc
endif

PLATFORM = -DPLATFORM_DESKTOP

# flags for building imFAP
FLAGS = -Wall -Wextra -pedantic -ggdb

SRCS = $(wildcard *.c)

OUTPUT_LIN = imfap
OUTPUT_WIN = imfap.exe

OUTPUT = $(OUTPUT_LIN)
ifeq ($(TARGET), Windows)
	OUTPUT = $(OUTPUT_WIN)
endif

LIBS = -lm
ifeq ($(TARGET), Windows)
	LIBS = -lopengl32 -lgdi32 -lwinmm
endif

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
	$(CC) -o $(OUTPUT) $(SRCS) $(FLAGS) -lraylib $(LIBS)

# build with provided version of raylib
build: $(SRCS) $(RAY_OBJS)
	$(CC) -o $(OUTPUT) $(SRCS) $(RAY_OBJS) -I$(RAY_PATH) $(FLAGS) $(LIBS)

clean:
	rm -f $(OUTPUT_LIN)
	rm -f $(OUTPUT_WIN)
	rm -f $(RAY_OBJS)
