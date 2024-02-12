# available targets: Linux, Windows, WEB
# (all build on Linux)
TARGET ?= Linux

CC = cc
PLATFORM = -DPLATFORM_DESKTOP
ifeq ($(TARGET), Windows)
	CC = x86_64-w64-mingw32-gcc
else ifeq ($(TARGET), WEB)
	CC = clang
	PLATFORM = -DPLATFORM_WASM
endif


# options given via environment variables.
# for ex.: 'TARGET=Windows FONT=0 make'
OPTIONS =
ifeq ($(FONT), 0)
	OPTIONS += -DDISABLE_CUSTOM_FONT
endif

# flags for building imFAP
FLAGS := -Wall -Wextra -pedantic -ggdb
ifeq ($(TARGET), WEB)
	STDLIB_32_INC = -I/usr/include/
	WASM_CFLAGS := --target=wasm32 $(STDLIB_32_INC) -nostdlib -O2
	WASM_LDFLAGS := -Wl,--allow-undefined -Wl,--no-entry -Wl,--lto-O2 -Wl,--export=maine  # note: main function can't be exported(?)
	FLAGS := $(WASM_CFLAGS) $(WASM_LDFLAGS)
endif

SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.c)

OUTPUT_LIN = imfap
OUTPUT_WIN = imfap.exe
OUTPUT_WEB = imfap.wasm

OUTPUT = $(OUTPUT_LIN)
ifeq ($(TARGET), Windows)
	OUTPUT = $(OUTPUT_WIN)
else ifeq ($(TARGET), WEB)
	OUTPUT = $(OUTPUT_WEB)
endif

LIBS = -lm
ifeq ($(TARGET), Windows)
	LIBS = -lopengl32 -lgdi32 -lwinmm
else ifeq ($(TARGET), WEB)
	LIBS =
endif

RAY_PATH = $(SRC_DIR)/external/raylib/src/
RAY_SRCS = $(wildcard $(addsuffix *.c, $(RAY_PATH)))
ifeq ($(TARGET), WEB)
	RAY_SRCS := $(filter-out $(RAY_PATH)rglfw.c, $(RAY_SRCS))
endif
RAY_OBJS = $(RAY_SRCS:c=o)

# flags for building raylib locally
CFLAGS = $(PLATFORM) -I$(RAY_PATH)external/glfw/include
ifeq ($(TARGET), WEB)
	CFLAGS := $(PLATFORM) $(WASM_CFLAGS)
endif

all: build

run: build
ifeq ($(TARGET), Windows)
	wine $(OUTPUT)
else
	./$(OUTPUT)
endif

# build from globally installed raylib library
build_from_global: $(SRCS)
	$(CC) -o $(OUTPUT) $(SRCS) $(FLAGS) $(OPTIONS) -lraylib $(LIBS)

# build with provided version of raylib
build: $(SRCS) $(RAY_OBJS)
	$(CC) -o $(OUTPUT) $(SRCS) $(RAY_OBJS) -I$(RAY_PATH) $(FLAGS) $(OPTIONS) $(LIBS)

clean:
	rm -f $(OUTPUT_LIN)
	rm -f $(OUTPUT_WIN)
	rm -f $(OUTPUT_WEB)
	rm -f $(RAY_OBJS)
