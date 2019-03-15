# This Makefile refreshes generated files and builds the gcode-cli tool used
# for parser development.  It is not necessary to build this module to run
# Klipper.
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

CC = gcc
BISON = bison
CFLAGS = -ggdb -fdiagnostics-color=always
OBJ = \
	gcode_cli.o \
	gcode_parser.generated.o \
	gcode_lexer.o \
	gcode_keywords.generated.o \
	gcode_ast.o

KEYWORD_DEPS = \
	gcode_parser.y \
	../gcode_parser.generated.c \
	gcode_keyword_gen.sh

.DEFAULT_GOAL := gcode-cli

../gcode_parser.generated.c: gcode_parser.y
	$(BISON) -Wall -v --report-file=gcode_parser.output -o $@ $<

../gcode_keywords.generated.c: $(KEYWORD_DEPS)
	./gcode_keyword_gen.sh

gcode_ast.o: ../gcode_ast.c
	$(CC) -c -o $@ $< $(CFLAGS)

gcode_lexer.o: ../gcode_lexer.c
	$(CC) -c -o $@ $< $(CFLAGS)

gcode_keywords.generated.o: ../gcode_keywords.generated.c
	$(CC) -c -o $@ $< $(CFLAGS)

gcode_parser.generated.o: ../gcode_parser.generated.c
	$(CC) -c -o $@ $< $(CFLAGS)

gcode-cli: $(OBJ)
	$(CC) -o $@ $^ -lm $(CFLAGS)

clean:
	rm -f *.o *.output *.gperf gcode-cli
