# G-Code parser & interpreter
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

GRAMMAR = """
    ?start: _LINE_NUMBER? command
        | empty_line
    empty_line: _LINE_NUMBER? _WS? _COMMENT?
    _LINE_NUMBER: /N[0-9]+/i
    ?command: raw_command
        | trad_command
        | ext_command

    // Raw command (arguments parsed as expression)
    raw_command: RAW_COMMAND_NAME (_WS raw_arg_segment*)?
    RAW_COMMAND_NAME.3: /(M112|ECHO)/i
    raw_arg_segment: RAW_ARG_TEXT -> str
        | expr_embed
        | STRING -> escaped_str
    RAW_ARG_TEXT: /[^"{]+/

    // Traditional command (arguments such as X0)
    trad_command: TRAD_COMMAND_NAME trad_param* _COMMENT?
    TRAD_COMMAND_NAME.2: /[A-Z][0-9]+/i
    trad_param: _WS trad_param_key "="? param_expr -> param
    trad_param_key: TRAD_PARAM_KEY_STR -> upper
        | expr_embed
        | STRING -> escaped_str
    TRAD_PARAM_KEY_STR: /[A-Z]/i

    // Extended command (arguments such as foo=bar)
    ext_command: EXT_COMMAND_NAME ext_param* _COMMENT?
    EXT_COMMAND_NAME.1: /[A-Z$_][A-Z0-9$_]*/i
    ext_param: _WS param_expr _WS? "=" _WS? param_expr -> param

    // Parameter expressions (extended keys or traditional/extended values)
    ?param_expr: param_expr_segment+
    param_expr_segment: EXPR_SEGMENT_TEXT -> upper
        | expr_embed
        | STRING -> escaped_str
    EXPR_SEGMENT_TEXT: /[^{"\s=;]+/

    // Comments
    _COMMENT: /\s*;.*/

    // Embedded expressions
    ?expr_embed: "{" expr "}"
    ?expr: NUMBER -> num
        | STRING -> escaped_str

    %import common.ESCAPED_STRING -> STRING
    %import common.SIGNED_NUMBER -> NUMBER
    %import common.WS_INLINE -> _WS
"""

from gcode import error
from ConfigParser import RawConfigParser
from StringIO import StringIO
from traceback import format_exception
from lark import Lark, Transformer, v_args
from lark.lexer import Lexer, Token
from lark.exceptions import UnexpectedInput, UnexpectedCharacters, \
    UnexpectedToken
from collections import deque
from numbers import Number
import ast

def _make_command(name, params):
    keys, values = zip(*params) if params else ((), ())
    params = ast.Dict(keys = list(keys), values = list(values))
    expr = ast.Tuple(elts = [ast.Str(str(name.upper())), params], ctx = ast.Load())
    ast.fix_missing_locations(expr)
    return compile(ast.Expression(expr), '<gcode>', 'eval')

class StatementExprBuilder(Transformer):
    number = ast.Num
    
    @staticmethod
    @v_args(inline = True)
    def str(s):
        return ast.Str(str(s))

    @staticmethod
    @v_args(inline = True)
    def upper(s):
        return ast.Str(s.upper())

    @staticmethod
    @v_args(inline = True)
    def raw_command(name, arg = None):
        return _make_command(name, [(ast.Str('*'), arg)] if arg else [])

    @staticmethod
    @v_args(inline = True)
    def trad_command(name, *args):
        return _make_command(name, args)

    @staticmethod
    @v_args(inline = True)
    def ext_command(name, *args):
        return _make_command(name, args)

    @staticmethod
    def outer_expr(items):
        expr = None
        for item in items:
            if expr:
                expr = item
            else:
                expr = BinOp(expr, ast.Add(), item)
        return expr

    @staticmethod
    @v_args(inline = True)
    def param(key, value):
        return (key, value)

    @staticmethod
    @v_args(inline = True)
    def escaped_str(str):
        return ast.Str(ast.literal_eval(str))

    @staticmethod
    def empty_line(*ignored):
        return None

class CommandEntry:
    def __init__(self, code):
        self.code = code
    def __call__(self, context, params):
        return eval(self.code, context, params)

class ErrorEntry:
    COMMAND_PREFIX = "  in command: "
    def __init__(self, exception, line):
        if isinstance(exception, UnexpectedCharacters):
            message = "Unexpected character '{}'".format(
                line[exception.column])
            if exception.allowed:
                message = "{} (expected '{}')".format(message,
                                                      exception.allowed)
        elif isinstance(exception, UnexpectedToken):
            expected = [ self._token_to_user(t) for t in exception.expected ]
            if len(expected) > 2:
                for i in range(expected.length - 2):
                    expected[i] += ", "
                expected[-2] += " or "
            token = self._token_to_user(exception.token)
            message = "Unexpected {} (expected {})".format(token,
                                                           ' '.join(expected))
        else:
            message = exception.message
        if exception.column != '?':
            here = "\n{}^ here".format(
                ' ' * (len(self.COMMAND_PREFIX) + exception.column - 1),
                exception.line)
        else:
            here = ''
        message = "{}\n{}{}{}".format(message, self.COMMAND_PREFIX, line, here)
        self.exception = error(message)
    def __call__(self, context, params):
        raise self.exception
    def _token_to_user(self, token):
        t = token.type if hasattr(token, 'type') else token
        if t == '$END':
            return 'end of line'
        if t == '_WS':
            return 'space'
        return str(token)

class Script:
    """Represents a single block of (possibly unterminated) G-Code input."""

    _parse = Lark(GRAMMAR, parser = 'lalr',
                  transformer = StatementExprBuilder,
                  lexer = 'contextual',
                  debug = True).parse

    def __init__(self, context = {}, data = None):
        """Initializes new script, optionally parsing G-Code in data arg."""
        self.entries = deque()
        self._fatal = None
        self._has_m112 = False
        self.context = context
        self.partial_line = None
        if data:
            self.parse_segment(data)
            self.parse_finish()
    
    def parse_segment(self, data):
        """Parse a partial G-Code input segment."""
        lines = data.split("\n")
        if self.partial_line:
            lines[0] = self.partial_line + lines[0]
            self.partial_line = None
        if lines and lines[-1]:
            self.partial_line = lines.pop()
        for line in lines:
            if not line:
                continue
            try:
                code = self._parse(line)
                if code:
                    self.entries.append(CommandEntry(code))
            except UnexpectedInput as e:
                self.entries.append(ErrorEntry(e, line))
    
    def parse_finish(self):
        """Parse any trailing data."""
        if self.partial_line:
            self.parse_segment("\n")

    def expose_config(self, config):
        """Expose configuration object for the G-Code 'config' parameter."""
        self.root_dict.expose_config(config)

    def expose_inputs(self, inputs):
        """Set parameters for the script."""
        self.root_dict.expose_inputs(inputs)

    def remove_inputs(self):
        """Remove script parameters."""
        self.root_dict.remove_inputs()

    def __len__(self):
        return len(self.entries)

    def eval_next(self, params = {}):
        if not self:
            return ()
        entry = self.entries.popleft()
        return entry(self.context, params)
