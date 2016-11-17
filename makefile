CC=gcc
CFLAGS=-O2 -ansi
DEBUG=-g -Wall -Wextra
LDFLAGS=-lcrypt -lsqlite3
SRC=src
INC=include
OBJ=obj

# make will build an .o in obj/ from every .c in src/
# executables and their main .c file must share the same name
OUTPUT=board.cgi submit.cgi

INPUT=$(wildcard $(SRC)/*.c)
OBJECTS=$(patsubst $(SRC)/%.c,$(OBJ)/%.o, $(INPUT))
MAINS=$(patsubst %.cgi,$(OBJ)/%.o, $(OUTPUT))

.PHONY: all release clean help

# target: all - default, rebuild outdated .o and relink .cgi
all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CC) -o $@ $(patsubst %.cgi,$(OBJ)/%.o, $@) $(filter-out $(MAINS), $^) $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c $(wildcard $(INC)/*.h)
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) $(DEBUG) -I$(INC) -c $< -o $@

# target: release - reset and build stripped binaries only
release: DEBUG = -D NDEBUG
release: CC += -s -march=native
release: clean all
	rm -rf $(OBJ)/

# target: clean - reset working directory
clean:
	rm -rf $(OBJ)/ $(OUTPUT)

# target: help - display available options
help:
	@grep "^# target:" [Mm]akefile | sed 's/# target:/make/g'
