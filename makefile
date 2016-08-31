CC=gcc
RFLAGS=-O2 -ansi -s
CFLAGS=-O2 -ansi -g -Wall -Wextra
LDFLAGS=-lcrypt -lsqlite3
SOURCE=src
INCLUDES=include
INPUT=$(wildcard $(SOURCE)/*.c)
OUTPUT=board.cgi

all:
	$(CC) $(CFLAGS) -o $(OUTPUT) -I$(INCLUDES) $(INPUT) $(LDFLAGS)

release:
	$(CC) $(RFLAGS) -o $(OUTPUT) -I$(INCLUDES) $(INPUT) $(LDFLAGS)

clean:
	rm -rf $(OUTPUT) *.out

dbclean:
	sudo rm -rf db/
