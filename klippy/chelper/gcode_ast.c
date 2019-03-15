// G-code abstract syntax tree implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_ast.h"
#include <stdlib.h>
#include <string.h>

#define GCODE_AST_ALLOC(Type) \
    GCode ## Type ## Node* n = malloc(sizeof(GCode ## Type ## Node)); \
    if (!n) \
        return NULL; \

#define GCODE_RETURN return (GCodeNode*)n;

GCodeNode* gcode_statement_new(GCodeNode* children) {
    GCODE_AST_ALLOC(Statement);
    n->children = children;
    return (GCodeNode*)n;
}

GCodeNode* gcode_parameter_new(const char* name) {
    GCODE_AST_ALLOC(Parameter);
    n->name = strdup(name);
    if (!n->name) {
        free(n);
        return NULL;
    }
    return (GCodeNode*)n;
}

GCodeNode* gcode_str_new(const char* text) {
    GCODE_AST_ALLOC(String);
    n->type = GCODE_STR;
    n->value = strdup(text);
    if (!n->value) {
        free(n);
        return NULL;
    }
    return (GCodeNode*)n;
}

GCodeNode* gcode_bool_new(bool value) {
    GCODE_AST_ALLOC(Bool);
    n->type = GCODE_BOOL;
    n->value = value;
    return (GCodeNode*)n;
}

GCodeNode* gcode_int_new(int64_t value) {
    GCODE_AST_ALLOC(Int);
    n->type = GCODE_INT;
    n->value = value;
    return (GCodeNode*)n;
}

GCodeNode* gcode_float_new(double value) {
    GCODE_AST_ALLOC(Float);
    n->type = GCODE_FLOAT;
    n->value = value;
    return (GCodeNode*)n;
}

GCodeNode* gcode_operator_new(gcode_operator_type_t type,
                              GCodeNode* children)
{
    GCODE_AST_ALLOC(Operator);
    n->type = GCODE_OPERATOR;
    n->operator = type;
    n->children = children;
    return (GCodeNode*)n;
}

GCodeNode* gcode_function_new(const char* name, GCodeNode* children) {
    GCODE_AST_ALLOC(Function);
    n->name = strdup(name);
    if (!n->name) {
        free(n);
        return NULL;
    }
    n->type = GCODE_FUNCTION;
    n->children = children;
    return (GCodeNode*)n;
}

GCodeNode* gcode_add_next(GCodeNode* sibling, GCodeNode* child) {
    if (!sibling)
        return child;
    while (sibling->next)
        sibling = sibling->next;
    sibling->next = child;
    return sibling;
}

GCodeNode* gcode_add_child(GCodeNode* parent, GCodeNode* child) {
    if (!parent || !child
        || (parent->type != GCODE_OPERATOR
            && parent->type != GCODE_FUNCTION
            && parent->type != GCODE_STATEMENT))
        return parent;
    GCodeParentNode* p = (GCodeParentNode*)parent;
    if (!p->children) {
        p->children = child;
        return parent;
    }
    GCodeNode* n = p->children;
    while (n->next)
        n = n->next;
    n->next = child;
    return parent;
}

void gcode_node_delete(GCodeNode* node) {
    if (!node)
        return;
    switch (node->type) {
        case GCODE_STATEMENT:
            gcode_node_delete(((GCodeStatementNode*)node)->children);
            break;

        case GCODE_PARAMETER:
            free(((GCodeParameterNode*)node)->name);
            break;

        case GCODE_STR:
            free(((GCodeStringNode*)node)->value);
            break;

        case GCODE_OPERATOR:
            gcode_node_delete(((GCodeOperatorNode*)node)->children);
            break;

        case GCODE_FUNCTION:
            free(((GCodeFunctionNode*)node)->name);
            gcode_node_delete(((GCodeFunctionNode*)node)->children);
            break;
    }
    gcode_node_delete(node->next);
    free(node);
}
