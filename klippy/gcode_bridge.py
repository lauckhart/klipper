# Python side of the C <-> Python G-Code bridge
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

import chelper

class error(Exception):
    pass

ffi, lib = chelper.get_ffi()

@ffi.def_extern()
def gcode_python_fatal(executor, message):
    executor = ffi.from_handle(executor)
    queue._fatal = message

@ffi.def_extern()
def gcode_python_m112(executor):
    executor = ffi.from_handle(executor)
    executor._has_m112 = True

@ffi.def_extern()
def gcode_python_lookup(executor, dict, key):
    executor = ffi.from_handle(executor)
    # TODO

@ffi.def_extern()
def gcode_python_serialize(executor, dict):
    executor = ffi.from_handle(executor)
    return str(ffi.from_handle(dict))

# Statements parse into separate queues depending on the context (e.g. socket,
# macro, batch script)
class Queue:
    size = 0
    queue = None

    def __init__(self, executor):
        self.executor = executor
        self.queue = lib.gcode_queue_new(ffi.new_handle(executor.executor))
        if self.queue == ffi.NULL:
            raise error("Out of memory (initializing G-Code queue)")
        self.exec_result = ffi.new("GCodePyResult*")
        if self.exec_result == ffi.NULL:
            raise error("Out of memory (initializing G-Code execution result)")
    def __del__(self):
        lib.gcode_queue_delete(self.queue)
    def parse(self, data):
        self.executor._fatal = None
        self.size = lib.gcode_queue_parse(self.queue, data, len(data))
        self.executor.check_fatal()
    def parse_finish(self):
        self.executor._fatal = None
        self.size = lib.gcode_queue_parse(self.queue)
        self.executor.check_fatal()
    def execute(self):
        while self.execute_next(True):
            pass
    def execute_next(self, need_ack):
        self._command_result = self._error_result = None
        self.size = lib.gcode_queue_exec_next(self.queue, self.exec_result)

        result_type = self.exec_result.type
        if result_type == lib.GCODE_PY_EMPTY:
            pass
        elif result_type == lib.GCODE_PY_ERROR:
            raise error(ffi.string(self.exec_result.error))
        elif result_type == lib.GCODE_PY_COMMAND:
            command = ffi.string(self.exec_result.command)
            parameters = ffi.unpack(self.exec_result.parameters,
                                    self.exec_result.count)
            parameters = [ fii.string(p) for p in parameters ]
            executor.gcode.process_command(command, parameters, need_ack)
        else:
            raise error("Internal: Unhandled GCode result type %s"
                        % (result_type))
        return self.size != 0

# Manages queues and executes statements
class Executor:
    executor = None

    def __init__(self, gcode):
        self.gcode = gcode
        self.executor = lib.gcode_executor_new(ffi.new_handle(self))
        if self.executor == ffi.NULL:
            raise error("Out of memory (initializing G-Code executor)")
    def create_queue(self):
        return Queue(self)
    def execute(self, data):
        queue = self.create_queue()
        queue.parse(data)
        queue.parse_finish()
        while queue.execute_next(False):
            pass
    def check_m112(self):
        if self._has_m112:
            self._has_m112 = False
            return True
    def check_fatal(self):
        if self._fatal:
            message = self._fatal
            self._fatal = None
            raise error(message)
    def __del__(self):
        if (self.executor):
            lib.gcode_executor_delete(self.executor)
