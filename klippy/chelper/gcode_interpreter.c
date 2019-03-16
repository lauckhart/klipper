// G-code interpreter implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_interpreter.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct GCodeInterpreter {
    void* context;

    const char** field_buf;
    size_t field_count;
    size_t field_limit;

    char* str_buf;
    size_t str_length;
    size_t str_limit;

    bool (*error)(void*, const char*, ...);
    bool (*lookup)(void*,  const char*, dict_handle_t, GCodeVal*);
    const char* (*serialize)(void*, dict_handle_t);
    bool (*exec)(const char**, size_t);
};

GCodeInterpreter* gcode_interp_new(
    void* context,
    bool (*error)(void*, const char*, ...),
    bool (*lookup)(void*, const char*, dict_handle_t, GCodeVal*),
    const char* (*serialize)(void*, dict_handle_t),
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
    interp->field_count = 0;
    interp->field_limit = 0;

    interp->str_buf = NULL;
    interp->str_length = 0;
    interp->str_limit = 0;
}

#define ERROR(args...) { \
    interp->error(interp->context, args); \
    return false; \
}

static bool str_expand(GCodeInterpreter* interp, size_t size) {
    if (interp->str_limit - interp->str_length < size) {
        size_t alloc_size = interp->str_limit < size
            ? interp->str_limit + interp->str_limit
            : interp->str_limit * 2;
        interp->str_buf = realloc(interp->str_buf, alloc_size);
        if (!interp->str_buf) {
            interp->str_length = 0;
            ERROR("Out of memory");
        }
    }
    return true;
}

char* gcode_interp_str_alloc(GCodeInterpreter* interp, size_t size) {
    if (!str_expand(interp, size + 1))
        return NULL;
    char* result = interp->str_buf + interp->str_length;
    interp->str_length += size + 1;
    return result;
}

const char* gcode_interp_printf(GCodeInterpreter* interp, const char* format,
                                ...)
{
    if (!interp->str_buf && !str_expand(interp, 512))
        return NULL;

    va_list argp;
    va_start(argp, format);
    size_t available = interp->str_limit - interp->str_length;
    int length = vsnprintf(interp->str_buf + interp->str_length,
                           interp->str_limit - interp->str_length,
                           format,
                           argp);
    if (length < 0) {
        va_end(argp);
        interp->error(interp->context,
                      "Internal: GCodeInterpreter printf failure");
        return NULL;
    }

    if (length > available) {
        if (!str_expand(interp, length)) {
            va_end(argp);
            return false;
        }
        length = vsnprintf(interp->str_buf + interp->str_length,
                           length,
                           format,
                           argp);
        if (length < 0) {
            va_end(argp);
            interp->error(interp->context,
                          "Internal: GCodeInterpreter printf failure");
            return false;
        }
    }

    va_end(argp);
    return gcode_interp_str_alloc(interp, length);
}

static void reset(GCodeInterpreter* interp) {
    interp->field_count = 0;
    interp->str_length = 0;
}

static bool eval(GCodeInterpreter* interp, GCodeNode* input, GCodeVal* output) {
    // TODO
    return true;
}

static inline const char* serialize(GCodeInterpreter* interp, GCodeVal* val) {
    switch (val->type) {
        GCODE_VAL_STR:
            return val->str_val;

        GCODE_VAL_BOOL:
            return val->bool_val ? "true" : "false";

        GCODE_VAL_INT:
            return gcode_interp_printf(interp, PRIu64, val->int_val);

        GCODE_VAL_FLOAT:
            return gcode_interp_printf(interp, "%f", val->float_val);

        GCODE_VAL_DICT:
            if (interp->serialize)
                return interp->serialize(interp->context, val->dict_val);
            else
                return "<obj>";

        default:
            interp->error(interp->context, "Internal: Unknown value type '%d'",
                          val->type);
            return NULL;
    }
}

static bool buffer_field(GCodeInterpreter* interp, const char* text) {
    if (interp->field_count == interp->field_limit) {
        size_t new_limit =
            interp->field_limit ? interp->field_limit * 2 : 0;
        interp->field_buf = realloc(interp->field_buf, new_limit);
        if (!interp->field_buf) {
            interp->field_count = interp->field_limit = 0;
            ERROR("Out of memory");
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
    for (GCodeNode* n = statement->children; n; n = n->next) {
        if (!eval(interp, n, &result))
            return false;
        const char* str = serialize(interp, &result);
        if (!str)
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
