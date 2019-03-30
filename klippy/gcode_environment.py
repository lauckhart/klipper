# Data interface between printer and G-Code
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

class StatusObjDict:
    """Exposes status of a single printer object to G-Code."""
    def __init__(self, parent, obj_name):
        self.parent = parent
        self.obj_name = obj_name

    def __getitem__(self, key):
        vals = self._vals(self.parent.eventtime())
        if vals and key in vals:
            return vals[key]

    def __str__(self):
        return self.serialize(self.parent.eventtime())

    def serialize(self, eventtime, prefix = ''):
        vals = self._vals(eventtime)
        if vals:
            return '\n'.join([ "{}{} = {}".format(prefix, k, v)
                             for k, v in vals.items() ])
        return ''

    def _vals(self, eventtime):
        obj = self.parent.obj(self.obj_name)
        if not obj:
            return
        return obj.get_status(eventtime)

class StatusDict:
    """Exposes internal object status to G-Code."""
    def __init__(self, printer):
        self.printer = printer
        self.obj_dicts = {}

    def __getitem__(self, key):
        if key in self.obj_dicts:
            return self.obj_dicts[key]
        obj = self.obj(key)
        if obj:
            obj_dict = StatusObjDict(self, key)
            self.obj_dicts[key] = obj_dict
            return obj_dict

    def obj(self, name):
        if self.printer:
            obj = self.printer.lookup_object(name, None)
            if obj and hasattr(obj, 'get_status'):
                return obj

    def eventtime(self):
        if self.printer:
            return self.printer.get_reactor().monotonic()

    def __str__(self):
        sections = []
        if self.printer:
            for name in self.printer.objects:
                obj_dict = self[name]
                if obj_dict:
                    str = obj_dict.serialize(self.eventtime(),
                        "{}.".format(name))
                    if str:
                        sections.append(str)
        return "\n\n".join(sections)

class ConfigSectionDict:
    """Exposes single configuration section to G-Code."""
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
            raw = RawConfigParser()
            raw.add_section(self.section_name)
            fc = self.parent.config.fileconfig
            for key, val in fc.items(self.section_name):
                raw.set(self.section_name, key, val)
            return self.parent.serialize(raw)

class ConfigDict:
    """Exposes top-level configuration to G-Code."""
    def __init__(self, config):
        self.sections = {}
        self.config = config

    def __getitem__(self, key):
        if (self.config.has_section(key)):
            if not key in self.sections:
                self.sections[key] = ConfigSectionDict(self, key)
            return self.sections[key]

    def __str__(self):
        return self.serialize(self.config.fileconfig)

    def serialize(self, rawconfig):
        buf = StringIO()
        rawconfig.write(buf)
        return buf.getvalue().strip()

class GlobalDict:
    """Provides global data for script evaluation."""
    def __init__(self, printer, config):
        self.status_dict = StatusDict(printer)
        self.config_dict = ConfigDict(config)
    def __getitem__(name):
        if name == "status":
            return self.status_dict
        if name == "config":
            return self.config_dict
