CC=gcc
CFLAGS=-O2 -ansi
DEBUG=-g -Wall -Wextra
LDFLAGS=-lcrypt -lsqlite3
SRC=src
INC=include
OBJ=obj

INPUT=$(wildcard $(SRC)/*.c)
OBJECTS=$(patsubst $(SRC)/%.c,$(OBJ)/%.o, $(INPUT))
OUTPUT=board.cgi

.PHONY: all release strip clean help

# target: all - default, rebuild outdated .o and relink
all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c $(wildcard $(INC)/*.h)
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) $(DEBUG) -I$(INC) -c $< -o $@

# target: release - reset and build stripped binary only
release: clean strip all
	rm -rf $(OBJ)/

strip:
	$(eval CC += -s)
	$(eval undefine DEBUG)

# target: clean - reset working directory
clean:
	rm -rf $(OUTPUT) $(OBJ)/

# target: help - display available options
help:
	@grep "^# target:" [Mm]akefile | sed 's/# target:/make/g'
