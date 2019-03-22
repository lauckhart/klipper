// Simplified interface for C <-> Python, definitions
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GCODE_BRIDGE
#define __GCODE_BRIDGE

#include <stddef.h>
#include <stdbool.h>

typedef struct GCodeQueue GCodeQueue;
typedef struct GCodeExecutor GCodeExecutor;

void gcode_python_fatal(void* queue, const char* error);
void gcode_python_m112(void* queue);
void gcode_python_error(void* executor, const char* message);
void gcode_python_exec(void* executor, const char* command,
                        const char** params, size_t count);
char* gcode_python_lookup(void* executor, void* dict, const char* key);
char* gcode_python_serialize(void* executor, void* dict);

GCodeQueue* gcode_queue_new(GCodeExecutor* executor);
void gcode_queue_parse_finish(GCodeQueue* queue);
bool gcode_queue_exec_next(GCodeQueue* executor);
void gcode_queue_delete(GCodeQueue* executor);
void gcode_queue_parse(GCodeQueue* queue, const char* buf, size_t length);

GCodeExecutor* gcode_executor_new(void* context);
void gcode_executor_delete(GCodeExecutor* executor);

#endif
