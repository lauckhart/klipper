// G-code interpreter public interface
//
// The interpreter ingests parsed statements and generates rows of raw gcode
// (pure text with all interpreted constructs removed).  Callers can register
// functions to perform (possibly recursive) environmental lookup.
//
// Current there is a 1:1 correlation between input statements and output
// lines.  This may change in the future.
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GCODE_INTERPRETER_H
#define __GCODE_INTERPRETER_H

#include "gcode_ast.h"

#include <stdbool.h>
#include <stddef.h>

typedef void* dict_handle_t;

// Type system for G-Code values produced by the interpreter
typedef enum gcode_val_type_t {
    GCODE_VAL_UNKNOWN,
    GCODE_VAL_STR,
    GCODE_VAL_BOOL,
    GCODE_VAL_INT,
    GCODE_VAL_FLOAT,
    GCODE_VAL_DICT
} gcode_val_type_t;

// Each value produced by the interpreter is encoded using this class.
typedef struct GCodeVal {
    gcode_node_type_t type;

    union {
        dict_handle_t dict_val;
        int64_t int_val;
        double float_val;
        const char* str_val;
        bool bool_val;
    };
} GCodeVal;

typedef struct GCodeInterpreter GCodeInterpreter;

// Instantiate a new interpreter.  All callbacks should return false on error.
//
// Args:
//     context - opaque handled used in all callbacks
//     error - error handling callback
//     lookup - dictionary lookup callback, handles foo.bar and foo["bar"].
//         If the child is not found, the callback should set the result type
//         to GCODE_VAL_UNKNOWN and return true
//     serialize - callback for serializing dicts
//     exec - called for each output line of raw G-Code
//
// Returns the new interpreter or NULL on OOM.
GCodeInterpreter* gcode_interp_new(
    void* context,
    bool (*error)(void* context, const char* text, ...),
    bool (*lookup)(void* context, const GCodeVal* key, dict_handle_t parent,
                   GCodeVal* result),
    const char* (*serialize)(void* context, dict_handle_t dict),
    bool (*exec)(const char** fields, size_t count)
);

// Allocate space on the interpreter string buffer.  No memory management is
// necessary for these strings but they only persist for the execution of a
// single statement.
//
// Args:
//     interp - the interpreter
//     size - # of bytes allocated, with one additional added for '\0'
//
// Returns a new string or NULL on fatal error.
char* gcode_interp_str_alloc(GCodeInterpreter* interp, size_t size);

// Allocate a new string on the intepreter string buffer and fill using printf.
//
// Args:
//     interp - the interpreter
//     format - print format
//     ... printf inputs
//
// Returns the new string or NULL on fatal error.
const char* gcode_interp_printf(GCodeInterpreter* interp, const char* format,
                                ...);

// Convert a GCodeVal to text.
//
// Args:
//     interp - the interpreter
//     val - the value to serialize
//
// Returns the stringified value or NULL on fatal error.
const char* gcode_str_cast(GCodeInterpreter* interp, const GCodeVal* val);

// Convert a GCodeVal to int64_t.
//
// Args:
//     interp - the interpreter
//     val - the value to convert
//
// Returns the integer value.  Cannot fail.
int64_t gcode_int_cast(const GCodeVal* val);

// Convert a GCodeVal to bool.
//
// Args:
//     interp - the interpreter
//     val - the value to convert
//
// Returns the bool value.  Cannot fail.
bool gcode_bool_cast(const GCodeVal* val);

// Convert a GCodeVal to double.
//
// Args:
//     interp - the interpreter
//     val - the value to convert
//
// Returns the double value.  Cannot fail.
double gcode_float_cast(const GCodeVal* val);

// Pass statements to the G-Code interpreter for execution.
//
// Args:
//     interp - the interpreter
//     statement - one or more statements chained via ->next
//
// Returns true on success.
bool gcode_interp_exec(GCodeInterpreter* interp,
                       const GCodeStatementNode* statement);

// Release all resources associated with an interpreter.
//
// Args:
//     interp - the interpreter
//
void gcode_interp_delete(GCodeInterpreter* interp);

#endif
