CC = gcc

all : main.o server.o

main.o : main.c
	$(CC) -o $@ $^ -lsqlite3

server.o : server.c
	$(CC) -o $@ $^ -lsqlite3

.PHONY: clean

clean:
	rm main.o
	rm server.o