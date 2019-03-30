#!/usr/bin/env python2
# Script to parse and dump interpreted gcode values
#
# Copyright (C) 2017 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

import os, sys

sys.path.append(os.path.join(os.path.dirname(__file__), '../klippy'))

from gcode import error
from gcode_script import Script

if len(sys.argv) != 2:
    sys.stderr.write("Usage: {} FILENAME\n".format(sys.argv[0]))
    sys.exit(1)

f = open(sys.argv[1])
data = f.read()
script = Script(data = data)
while script:
    try:
        command, parameters = script.eval_next({ "foo": "bar" })
        print "{} {}".format(command, ' '.join([
            "{}={}".format(k, v) for k, v in parameters.items()
        ]))
    except error as e:
        print('\n'.join([ "* {}".format(l) for l in e.message.split("\n")]))
