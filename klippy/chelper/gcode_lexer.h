// G-code lexer public interface
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct GCodeLexerSession GCodeLexerSession;
typedef int8_t gcode_keyword_t;

GCodeLexerSession* gcode_lexer_start(
    void* context,
    bool (*error)(void*, const char* format, ...),
    bool (*keyword)(void*, gcode_keyword_t id),
    bool (*identifier)(void*, const char* value),
    bool (*string_literal)(void*, const char* value),
    bool (*integer_literal)(void*, int64_t value),
    bool (*float_literal)(void*, double value)
);
bool gcode_lexer_lex(GCodeLexerSession* session, const char* buffer,
                     size_t length);
void gcode_lexer_finish(GCodeLexerSession* session);
