// G-code abstract syntax tree public interface
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef GCODE_AST_H
#define GCODE_AST_H

#include <stdint.h>

typedef enum gcode_node_type_t {
    GCODE_UNKNOWN_NODE,
    GCODE_IDENTIFIER,
    GCODE_STR,
    GCODE_INT,
    GCODE_FLOAT,
    GCODE_OPERATOR,
    GCODE_CALL
} gcode_node_type_t;

typedef enum gcode_operator_type_t {
    GCODE_UNKNOWN_OPERATOR,
    GCODE_AND,
    GCODE_EQUAL,
    GCODE_CONCAT,
    GCODE_PLUS,
    GCODE_MINUS,
    GCODE_MOD,
    GCODE_TIMES,
    GCODE_DIVIDE,
    GCODE_LT,
    GCODE_GT,
    GCODE_LTE,
    GCODE_GTE,
    GCODE_NOT,
    GCODE_IFELSE,
    GCODE_LOOKUP
} gcode_operator_type_t;

typedef struct GCodeNode {
    gcode_node_type_t type;
    struct GCodeNode* next;
} GCodeNode;

typedef struct GCodeIdentifierNode {
    gcode_node_type_t type;
    GCodeNode* next;
    char* name;
} GCodeIdentifierNode;

typedef struct GCodeStrNode {
    gcode_node_type_t type;
    GCodeNode* next;
    char* value;
} GCodeStringNode;

typedef struct GCodeIntNode {
    gcode_node_type_t type;
    GCodeNode* next;
    int64_t value;
} GCodeIntNode;

typedef struct GCodeFloatNode {
    gcode_node_type_t type;
    GCodeNode* next;
    double value;
} GCodeFloatNode;

typedef struct GCodeOperatorNode {
    gcode_node_type_t type;
    GCodeNode* next;
    gcode_operator_type_t operator;
    GCodeNode* children;
} GCodeOperatorNode;

typedef struct GCodeCallNode {
    gcode_node_type_t type;
    GCodeNode* next;
    char* name;
    GCodeNode* children;
} GCodeCallNode;

GCodeNode* gcode_identifier_new(const char* text);
GCodeNode* gcode_str_new(const char* text);
GCodeNode* gcode_int_new(int64_t value);
GCodeNode* gcode_float_new(double value);
GCodeNode* gcode_operator_new(gcode_operator_type_t type, GCodeNode* children);
GCodeNode* gcode_call_new(const char* name, GCodeNode* children);
void gcode_node_delete(GCodeNode* node);

#endif
