# Python side of the C <-> Python G-Code bridge
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

import chelper, gcode, numbers, ConfigParser, StringIO

ffi, lib = chelper.get_ffi()

class Dict:
    pass

class ConfigSectionDict(Dict):
    def __init__(self, parent, section_name):
        self.parent = parent
        self.section_name = section_name
    def __getitem__(self, key):
        fc = self.parent.config.fileconfig
        if fc.has_option(self.section_name, key):
            return fc.get(self.section_name, key)
    def __str__(self):
        c = self.parent.config
        if c.has_section(self.section_name):
            raw = ConfigParser.RawConfigParser()
            raw.add_section(self.section_name)
            fc = self.parent.config.fileconfig
            for key, val in fc.items(self.section_name):
                raw.set(self.section_name, key, val)
            return self.parent.serialize(raw)

class ConfigDict(Dict):
    def __init__(self, executor):
        self.executor = executor
        self.sections = {}
    def __getitem__(self, key):
        if (self.config.has_section(key)):
            if not key in self.sections:
                self.sections[key] = ConfigSectionDict(self, key)
            return self.sections[key]
    def __str__(self):
        return self.serialize(self.config.fileconfig)
    def serialize(self, rawconfig):
        buf = StringIO.StringIO()
        rawconfig.write(buf)
        return buf.getvalue().strip()

class RootDict(Dict):
    def __init__(self, executor):
        self.config_dict = ConfigDict(executor)
    def __getitem__(self, key):
        if key == "config":
            return self.config_dict

@ffi.def_extern()
def gcode_python_fatal(executor, message):
    executor = ffi.from_handle(executor)
    executor._fatal = gcode.fatal(message)

@ffi.def_extern()
def gcode_python_m112(executor):
    executor = ffi.from_handle(executor)
    executor._has_m112 = True

@ffi.def_extern()
def gcode_python_lookup(executor, dict, key, result):
    executor = ffi.from_handle(executor)
    key = ffi.string(key)
    if dict == ffi.NULL:
        dict = executor.root_dict
    else:
        dict = ffi.from_handle(dict)
    value = dict[key]
    if value != None:
        if isinstance(value, bool):
            result.type = lib.GCODE_VAL_BOOL
            resutl.bool_val = value
        elif isinstance(value, numbers.Number):
            result.type = lib.GCODE_VAL_FLOAT
            result.float_val = float(value)
        elif isinstance(value, Dict):
            if not hasattr(value, '_ffi_handle'):
                value._ffi_handle = ffi.new_handle(value)
            result.type = lib.GCODE_VAL_DICT
            result.dict_val = value._ffi_handle
        else:
            result.type = lib.GCODE_VAL_STR
            result.str_val = executor.c_str(value)

@ffi.def_extern()
def gcode_python_serialize(executor, dict):
    executor = ffi.from_handle(executor)
    return executor.c_str(str(ffi.from_handle(dict)))

# Statements parse into separate queues depending on the context (e.g. socket,
# macro, batch script)
class Queue:
    size = 0
    c_queue = None

    def __init__(self, executor):
        self.executor = executor
        self.c_queue = lib.gcode_queue_new(executor.c_executor)
        if self.c_queue == ffi.NULL:
            raise gcode.fatal("Out of memory (initializing G-Code c_queue)")
        self.exec_result = ffi.new("GCodePyResult*")
        if self.exec_result == ffi.NULL:
            raise gcode.fatal(
                "Out of memory (initializing G-Code execution result)")
    def __del__(self):
        lib.gcode_queue_delete(self.c_queue)
    def parse(self, data):
        self.executor._fatal = None
        self.size = lib.gcode_queue_parse(self.c_queue, data, len(data))
        self.executor.check_fatal()
    def parse_finish(self):
        self.executor._fatal = None
        self.size = lib.gcode_queue_parse(self.c_queue)
        self.executor.check_fatal()
    def has_next(self):
        return self.size > 0
    def next(self):
        self._command_result = self._error_result = None
        self.size = lib.gcode_queue_exec_next(self.c_queue, self.exec_result)
        result_type = self.exec_result.type
        if result_type == lib.GCODE_PY_ERROR:
            raise gcode.error(ffi.string(self.exec_result.error))
        if result_type == lib.GCODE_PY_COMMAND:
            command = ffi.string(self.exec_result.command)
            if self.exec_result.count:
                parameters = ffi.unpack(self.exec_result.parameters,
                                        self.exec_result.count)
                parameters = {
                    ffi.string(parameters[i]): ffi.string(parameters[i + 1])
                        for i in range(0, len(parameters), 2)
                }
            else:
                parameters = {}
            return (command, parameters)
        raise gcode.fatal("Internal: Unhandled GCode result type %s"
                          % (result_type))

# Manages queues and executes statements
class Executor:
    c_executor = None

    def __init__(self, gcode):
        self.gcode = gcode
        self.handle = ffi.new_handle(self)
        self.c_executor = lib.gcode_executor_new(self.handle)
        if self.c_executor == ffi.NULL:
            raise fatal("Out of memory (initializing G-Code c_executor)")
        self.root_dict = RootDict(self)
    def expose_config(self, config):
        self.root_dict.config_dict.config = config
    def create_queue(self):
        return Queue(self)
    def parse(self, data):
        queue = self.create_queue()
        queue.parse(data)
        queue.parse_finish()
        return queue
    def check_m112(self):
        if self._has_m112:
            self._has_m112 = False
            return True
    def check_fatal(self):
        if self._fatal:
            err = self._fatal
            self._fatal = None
            raise err
    def c_str(self, value):
        result = lib.gcode_executor_str(self.c_executor, str(value))
        return result
    def __del__(self):
        if (self.c_executor):
            lib.gcode_executor_delete(self.c_executor)
