// G-code lexer public interface
//
// This is an incremental single-pass lexer that performs minimal heap
// allocation.
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GCODE_LEXER_H
#define __GCODE_LEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Structure for tracking source locations
typedef struct GCodeLocation {
    uint32_t first_line;
    uint32_t first_column;
    uint32_t last_line;
    uint32_t last_column;
} GCodeLocation;

typedef struct GCodeLexer GCodeLexer;
typedef int16_t gcode_keyword_t;

// Defined in Python
bool lex_error(void* context, const char* message);
bool lex_keyword(void* context, const char* value);
bool lex_identifier(void* context, const char* value);
bool lex_str_literal(void* context, const char* value);
bool lex_int_literal(void* context, int64_t value);
bool lex_float_literal(void* context, double value);
bool lex_bridge(void* context);
bool lex_end_statement(void* context);

// Instantiate a new lexer.  All callbacks should return false on error.  This
// puts the lexer into error parsing state, where all tokens are ignored until
// end-of-statement.
//
// Args:
//     context - an opaque handle passed to all callbacks
//     location - optional positional tracking
//
// Returns the new lexer or NULL on fatal error.
GCodeLexer* gcode_lexer_new(void* context, struct GCodeLocation* location);

// Tokenize a string.  Lexical state persists between calls so buffer may
// terminate anywhere in a statement.  Error handling occurs via the lexer
// error callback.
//
// Args:
//     lexer - the lexer
//     buffer - pointer to characters to scan
//     length - length of the buffer
void gcode_lexer_scan(GCodeLexer* lexer, const char* buffer, size_t length);

// Reset lexical state.  After the call the lexer may be reused.
//
// Args:
//     lexer - the lexer
void gcode_lexer_reset(GCodeLexer* lexer);

// Terminate lexing and trigger remaining callbacks.
//
// Args:
//     lexer - the lexer
void gcode_lexer_finish(GCodeLexer* lexer);

// Release lexer resources
//
// Args:
//     lexer - the lexer
void gcode_lexer_delete(GCodeLexer* lexer);

#endif
