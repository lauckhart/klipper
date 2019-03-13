// G-code lexer based on re2c
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdbool.h>
#include <stdint.h>

typedef struct GCodeLexerSession GCodeLexerSession;

GCodeLexerSession* gcode_lexer_start(
    void* context,
    bool (*error)(void*, const char* format, ...),
    bool (*token)(void*, const char* token),
    bool (*string_literal)(void*, const char* value),
    bool (*integer_literal)(void*, int64_t value),
    bool (*float_literal)(void*, double value)
);
bool gcode_lexer_lex(GCodeLexerSession* session, const char* buffer,
                     size_t length);
void gcode_lexer_finish(GCodeLexerSession* session);
