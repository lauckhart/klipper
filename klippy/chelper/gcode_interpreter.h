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

typedef enum gcode_val_type_t {
    GCODE_VAL_UNKNOWN,
    GCODE_VAL_STR,
    GCODE_VAL_BOOL,
    GCODE_VAL_INT,
    GCODE_VAL_FLOAT,
    GCODE_VAL_DICT
} gcode_val_type_t;

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

GCodeInterpreter* gcode_interp_new(
    void* context,
    bool (*error)(void* context, const char* text, ...),
    bool (*lookup)(void* context, const GCodeVal* key, dict_handle_t parent,
                   GCodeVal* result),
    const char* (*serialize)(void* context, dict_handle_t dict),
    bool (*exec)(const char** fields, size_t count)
);
char* gcode_interp_str_alloc(GCodeInterpreter* interp, size_t size);
const char* gcode_interp_printf(GCodeInterpreter* interp, const char* format,
                                ...);
const char* gcode_str_cast(GCodeInterpreter* interp, const GCodeVal* val);
int64_t gcode_int_cast(const GCodeVal* val);
bool gcode_bool_cast(const GCodeVal* val);
double gcode_float_cast(const GCodeVal* val);
bool gcode_interp_exec(GCodeInterpreter* interp,
                       const GCodeStatementNode* statement);
void gcode_interp_delete(GCodeInterpreter* interp);

#endif
