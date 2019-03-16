// G-code abstract syntax tree public interface
//
// This AST is the output type for GCodeParser.
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GCODE_AST_H
#define __GCODE_AST_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef enum gcode_node_type_t {
    GCODE_UNKNOWN_NODE,
    GCODE_STATEMENT,
    GCODE_PARAMETER,
    GCODE_STR,
    GCODE_BOOL,
    GCODE_INT,
    GCODE_FLOAT,
    GCODE_OPERATOR,
    GCODE_FUNCTION
} gcode_node_type_t;

typedef enum gcode_operator_type_t {
    GCODE_UNKNOWN_OPERATOR,
    GCODE_AND,
    GCODE_OR,
    GCODE_EQUALS,
    GCODE_CONCAT,
    GCODE_ADD,
    GCODE_SUBTRACT,
    GCODE_MODULUS,
    GCODE_POWER,
    GCODE_MULTIPLY,
    GCODE_DIVIDE,
    GCODE_LT,
    GCODE_GT,
    GCODE_LTE,
    GCODE_GTE,
    GCODE_NOT,
    GCODE_NEGATE,
    GCODE_IFELSE,
    GCODE_LOOKUP
} gcode_operator_type_t;

typedef struct GCodeNode {
    gcode_node_type_t type;
    struct GCodeNode* next;
} GCodeNode;

typedef struct GCodeParentNode {
    gcode_node_type_t type;
    GCodeNode* next;
    GCodeNode* children;
} GCodeParentNode;

typedef struct GStatementNode {
    gcode_node_type_t type;
    struct GCodeStatementNode* next;
    GCodeNode* children;
} GCodeStatementNode;

typedef struct GCodeParameterNode {
    gcode_node_type_t type;
    GCodeNode* next;
    const char* name;
} GCodeParameterNode;

typedef struct GCodeStrNode {
    gcode_node_type_t type;
    GCodeNode* next;
    const char* value;
} GCodeStrNode;

typedef struct GCodeBoolNode {
    gcode_node_type_t type;
    GCodeNode* next;
    bool value;
} GCodeBoolNode;

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
    GCodeNode* children;
    gcode_operator_type_t operator;
} GCodeOperatorNode;

typedef struct GCodeFunctionNode {
    gcode_node_type_t type;
    GCodeNode* next;
    GCodeNode* children;
    const char* name;
} GCodeFunctionNode;

static inline size_t gcode_node_length(const GCodeNode* node) {
    size_t l = 0;
    for (; node; node = node->next)
        l++;
    return l;
}

static inline GCodeNode* gcode_statement_new(GCodeNode* children) {
    GCodeStatementNode* n = malloc(sizeof(GCodeStatementNode));
    if (!n)
        return NULL;
    n->children = children;
    return (GCodeNode*)n;
}

static inline GCodeNode* gcode_str_new(const char* value) {
    if (!value)
        return NULL;
    size_t l = strlen(value);
    GCodeStrNode* n = malloc(sizeof(GCodeStrNode) + l + 1);
    if (!n)
        return NULL;
    n->type = GCODE_STR;
    n->value = (char*)(n + 1);
    strncpy((char*)(n + 1), value, l + 1);
    return (GCodeNode*)n;
}

static inline bool gcode_is_parent_node(const GCodeNode* node) {
    switch (node->type) {
        case GCODE_FUNCTION:
        case GCODE_OPERATOR:
        case GCODE_STATEMENT:
            return true;
    }
    return false;
}

static inline const GCodeNode* gcode_next(const GCodeNode* node) {
    if (!node)
        return NULL;
    return node->next;
}

GCodeNode* gcode_parameter_new(const char* name);
GCodeNode* gcode_bool_new(bool value);
GCodeNode* gcode_int_new(int64_t value);
GCodeNode* gcode_float_new(double value);
GCodeNode* gcode_operator_new(gcode_operator_type_t type, GCodeNode* children);
GCodeNode* gcode_function_new(const char* name, GCodeNode* children);
GCodeNode* gcode_add_next(GCodeNode* sibling, GCodeNode* next);
GCodeNode* gcode_add_child(GCodeNode* parent, GCodeNode* child);
void gcode_node_delete(GCodeNode* node);

#endif
