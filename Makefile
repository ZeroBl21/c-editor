build: main.c
	@$(CC) main.c -o ./bin/kilo -Wall -Wextra -pedantic -std=c99

run: build
	./bin/kilo
