CC = gcc
C_LINKS = -I src
C_FLAGS = -Wall -Wextra -pedantic -std=c99
SRC = main.c src/*.c
OUT = ./bin/kilo

build: main.c
	@$(CC) $(SRC) -o $(OUT) $(C_FLAGS) $(C_FLAGS)

run: build
	./bin/kilo $(FILE)

clean:
	rm -f $(OUT)
