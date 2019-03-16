// G-code parser public interface
//
// The parser incrementally ingests text and outputs parsed G-code statements.
// Input buffers need not be aligned on newlines.
//
// Output is in the form of GCodeNode lists.  Output statements can be
// evaluated using a GCodeInterpreter.  The recipient must free statements via
// gcode_node_delete.
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GCODE_PARSER_H
#define __GCODE_PARSER_H

#include "gcode_ast.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct GCodeParser GCodeParser;

GCodeParser* gcode_parser_new(
    void* context,
    bool (*error)(void* context, const char* text),
    bool (*statements)(void* context, GCodeStatementNode* statements)
);
bool gcode_parser_parse(GCodeParser* parser, const char* buffer,
                        size_t length);
void gcode_parser_delete(GCodeParser* parser);

#endif
