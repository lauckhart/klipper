// G-code parser public interface
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

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
