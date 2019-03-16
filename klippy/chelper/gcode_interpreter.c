// G-code interpreter implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_interpreter.h"

#include <stddef.h>

struct GCodeInterpreter {
    void* context;

    const char** field_buf;
    size_t field_count;
    size_t field_limit;

    bool (*error)(void*, const char*, ...);
    bool (*lookup)(void*,  const char*, dict_handle_t, GCodeVal*);
    bool (*serialize)(void*, dict_handle_t, GCodeVal*);
    bool (*exec)(const char**, size_t);
};

GCodeInterpreter* gcode_interp_new(
    void* context,
    bool (*error)(void*, const char*, ...),
    bool (*lookup)(void*, const char*, dict_handle_t, GCodeVal*),
    bool (*serialize)(void*, dict_handle_t, GCodeVal*),
    bool (*exec)(const char**, size_t))
{
    GCodeInterpreter* interp = malloc(sizeof(GCodeInterpreter));
    if (!interp) {
        error(context, "Out of memory");
        return NULL;
    }

    interp->context = context;
    interp->error = error;
    interp->lookup = lookup;
    interp->serialize = serialize;

    interp->field_buf = NULL;
    interp->field_limit = 0;
    interp->field_count = 0;
}

#define ERROR(args...) { \
    interp->error(interp->context, args); \
    return false; \
}

static void reset(GCodeInterpreter* interp) {
    interp->field_count = 0;
}

static bool eval(GCodeInterpreter* interp, GCodeNode* input, GCodeVal* output) {
    // TODO
    return true;
}

static bool serialize(GCodeInterpreter* interp, GCodeVal* node,
                      const char** output)
{
    // TODO
    return true;
}

static bool buffer_field(GCodeInterpreter* interp, const char* text) {
    if (interp->field_count == interp->field_limit) {
        size_t new_limit =
            interp->field_limit ? interp->field_limit * 2 : 0;
        interp->field_buf = realloc(interp->field_buf, new_limit);
        if (!interp->field_buf) {
            interp->error(interp->context, "Out of memory");
            interp->field_count = interp->field_limit = 0;
            return false;
        }
    }

    interp->field_buf[interp->field_count++] = text;
    return true;
}

bool gcode_interp_exec(GCodeInterpreter* interp,
                       GCodeStatementNode* statement)
{
    reset(interp);

    GCodeVal result;
    const char* str;
    for (GCodeNode* n = statement->children; n; n = n->next) {
        if (!eval(interp, n, &result))
            return false;
        if (!serialize(interp, &result, &str))
            return false;
        if (!buffer_field(interp, str))
            return false;
    }

    if (!interp->exec(interp->field_buf, interp->field_count))
        return false;

    return true;
}

void gcode_interp_delete(GCodeInterpreter* interp) {
    free(interp->field_buf);
    free(interp);
}
