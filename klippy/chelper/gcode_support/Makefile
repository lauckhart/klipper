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
VPATH = .:..
SOURCES = \
	gcode_cli.c \
    gcode_interpreter.c \
	gcode_parser.generated.c \
	gcode_lexer.c \
	gcode_keywords.generated.c \
	gcode_ast.c \
	gcode_error.c

KEYWORD_DEPS = \
	gcode_parser.y \
	../gcode_parser.generated.c \
	gcode_keyword_gen.sh

.DEFAULT_GOAL := out/gcode-cli

../gcode_parser.generated.c: gcode_parser.y
	$(BISON) -Wall -v --report-file=out/gcode_parser.output -o $@ $<

../gcode_keywords.generated.c: $(KEYWORD_DEPS)
	./gcode_keyword_gen.sh

out:
	mkdir out

out/depend: $(SOURCES) Makefile
	mkdir -p out
	rm -f out/depend
	$(CC) $(CFLAGS) -MM $(filter-out Makefile, $^) | sed -e 's/\(.*\.o\)/out\/\1/' > out/depend

out/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

out/gcode-cli: out/depend $(addprefix out/, $(SOURCES:.c=.o))
	$(CC) -o $@ $(filter-out out/depend, $^) -lm $(CFLAGS) -fsanitize=leak

clean:
	rm -f out/*
	rmdir out

-include out/depend
