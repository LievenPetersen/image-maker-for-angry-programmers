
libs = -lraylib -lm
flags = -Wall -Wextra -pedantic -ggdb #-Werror
srcs = $(wildcard *.c)
output = main

all: build

run: build
	./$(output)

build: $(srcs)
	gcc -o $(output) $(srcs) $(flags) $(libs)

clean:
	rm $(output)
