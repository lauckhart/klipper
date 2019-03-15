// G-code abstract syntax tree implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_ast.h"
#include <stdlib.h>
#include <string.h>

GCodeNode* gcode_identifier_new(const char* name) {
    GCodeIdentifierNode* n = malloc(sizeof(GCodeIdentifierNode));
    if (!n)
        return NULL;
    n->name = strdup(name);
    if (!n->name) {
        free(n);
        return NULL;
    }
    return (GCodeNode*)n;
}

GCodeNode* gcode_str_new(const char* text) {
    GCodeStringNode* n = malloc(sizeof(GCodeStringNode));
    if (!n)
        return NULL;
    n->type = GCODE_STR;
    n->value = strdup(text);
    if (!n->value) {
        free(n);
        return NULL;
    }
    return (GCodeNode*)n;
}

GCodeNode* gcode_int_new(int64_t value) {
    GCodeIntNode* n = malloc(sizeof(GCodeIntNode));
    if (!n)
        return NULL;
    n->type = GCODE_INT;
    n->value = value;
    return (GCodeNode*)n;
}

GCodeNode* gcode_float_new(double value) {
    GCodeFloatNode* n = malloc(sizeof(GCodeFloatNode));
    if (!n)
        return NULL;
    n->type = GCODE_FLOAT;
    n->value = value;
    return (GCodeNode*)n;
}

GCodeNode* gcode_operator_new(gcode_operator_type_t type,
                              GCodeNode* children)
{
    GCodeOperatorNode* n = malloc(sizeof(GCodeOperatorNode));
    if (!n)
        return NULL;
    n->type = GCODE_OPERATOR;
    n->operator = type;
    n->children = children;
    return (GCodeNode*)n;
}

GCodeNode* gcode_call_new(const char* name, GCodeNode* children) {
    GCodeCallNode* n = malloc(sizeof(GCodeCallNode));
    if (!n)
        return NULL;
    n->name = strdup(name);
    if (!n->name) {
        free(n);
        return NULL;
    }
    n->type = GCODE_CALL;
    n->children = children;
    return (GCodeNode*)n;
}

void gcode_node_delete(GCodeNode* node) {
    if (!node)
        return;
    switch (node->type) {
        case GCODE_IDENTIFIER:
            free(((GCodeIdentifierNode*)node)->name);
            break;

        case GCODE_STR:
            free(((GCodeStringNode*)node)->value);
            break;

        case GCODE_OPERATOR:
            gcode_node_delete(((GCodeOperatorNode*)node)->children);
            break;

        case GCODE_CALL:
            free(((GCodeCallNode*)node)->name);
            gcode_node_delete(((GCodeCallNode*)node)->children);
            break;
    }
    gcode_node_delete(node->next);
    free(node);
}
