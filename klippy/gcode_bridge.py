# Python side of the C <-> Python G-Code bridge
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
import chelper

ffi, lib = chelper.get_ffi()

@ffi.def_extern()
def gcode_python_fatal(executor, message):
    executor = ffi.from_handle(executor)
    queue._fatal = message

@ffi.def_extern()
def gcode_python_m112(executor):
    queue = ffi.from_handle(executor)
    executor._has_m112 = True

@ffi.def_extern()
def gcode_python_error(executor, message):
    executor = ffi.from_handle(executor)
    executor._error_result = message

@ffi.def_extern()
def gcode_python_exec(executor, command, params, count):
    executor = ffi.from_handle(executor)
    executor._command_result = (command, params)

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
    def __initialize__(self, executor):
        self.executor = executor
        self.queue = lib.gcode_queue_new(ffi.new_handle(self,
                                                        executor.executor))
        if self.queue == ffi.NULL:
            raise "Out of memory (initializing G-Code queue)"
    def __del__(self):
        lib.gcode_queue_delete(self.queue)
    def parse(self, data):
        self.executor._fatal = None
        lib.gcode_queue_parse(self.queue, data, len(data))
        self.executor.check_fatal()
    def parse_finish(self):
        self.executor._fatal = None
        lib.gcode_queue_parse(self.queue)
        self.executor.check_fatal()
    def size(self):
        return self.queue.size
    def execute(self):
        while self.executor.execute_next(self, True):
            pass

# Manages queues and executes statements
class Executor:
    def __initialize__(self, gcode):
        self.gcode = gcode
        self.executor = lib.gcode_bridge_new(ffi.new_handle(self))
        if self.bridge == ffi.NULL:
            raise "Out of memory (initializing G-Code executor)"
    def create_queue():
        return Queue(self)
    def execute(self, data):
        queue = self.create_queue()
        queue.parse(data)
        queue.parse_finish()
        while execute_next(queue, False):
            pass
    def execute_next(self, queue, need_ack):
        self._command_result = self._error_result = None
        has_more = lib.gcode_queue_exec_next(queue)
        if self._error_result:
            raise self._error_result
        if self.statement_result:
            gcode.process_command(self._statement_result.command,
                                  self._statement_result.params,
                                  need_ack)
        else:
            raise "Internal: No error or command emitted for statement"
        return has_more
    def check_m112():
        if self._has_m112:
            self._has_m112 = False
            return True
    def check_fatal():
        if self._fatal:
            message = self._fatal
            self._fatal = None
            raise message
    def __del__(self):
        lib.gcode_executor_delete(self.executor)
