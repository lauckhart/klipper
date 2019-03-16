// G-code lexer public interface
//
// This is an incremental single-pass lexer that performs minimal heap
// allocation.  It is used internally by gcode_parser.
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GCODE_LEXER_H
#define __GCODE_LEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct GCodeLexer GCodeLexer;
typedef int16_t gcode_keyword_t;

GCodeLexer* gcode_lexer_new(
    void* context,
    bool (*error)(void* context, const char* format, ...),
    bool (*keyword)(void* context, gcode_keyword_t id),
    bool (*identifier)(void* context, const char* name),
    bool (*string_literal)(void* context, const char* value),
    bool (*int_literal)(void* context, int64_t value),
    bool (*float_literal)(void* context, double value)
);
bool gcode_lexer_scan(GCodeLexer* lexer, const char* buffer,
                      size_t length);
void gcode_lexer_reset(GCodeLexer* lexer);
void gcode_lexer_finish(GCodeLexer* lexer);
void gcode_lexer_delete(GCodeLexer* lexer);

#endif
