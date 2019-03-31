# G-Code parser & interpreter
#
# Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

# Grammar notes
#
# Expression rules group patterns that are the same precedence.  Each group
# references the next highest precedence group.  This is how you encode
# precedence in EBNF.
#
# Whitespace is ignored within embedded expressions (see postlex Lark option).
# We need to do this to avoid _WS matches that would make the grammar ambiguous
# due to LALR single-token lookahead.  But we can't do it everywhere because
# whitespace is the only token separator in G-Code.

GRAMMAR = """
    ?start: _LINE_NUMBER? command
        | empty_line
    empty_line: _LINE_NUMBER? _WS? _COMMENT?
    ?command: raw_command | trad_command | ext_command

    // Raw command (arguments parsed as expression)
    raw_command: RAW_COMMAND_NAME (_WS raw_arg_segment*)? _WS? _COMMENT?
    RAW_COMMAND_NAME.3: /(M112|ECHO)/i
    raw_arg_segment: RAW_ARG_TEXT -> str
        | expr_embed
        | STRING -> escaped_str
    RAW_ARG_TEXT: /[^"{]+/

    // Traditional command (arguments such as X0)
    trad_command: TRAD_COMMAND_NAME trad_param* _WS? _COMMENT?
    TRAD_COMMAND_NAME.2: /[A-Z][0-9]+/i
    trad_param: _WS trad_param_key param_expr -> param
    trad_param_key: TRAD_PARAM_KEY_STR -> upper
        | expr_embed
        | STRING -> escaped_str
    TRAD_PARAM_KEY_STR: /[A-Z]/i

    // Extended command (arguments such as foo=bar)
    ext_command: EXT_COMMAND_NAME ext_param* _WS? _COMMENT?
    EXT_COMMAND_NAME.1: /[A-Z$_][A-Z0-9$_]*/i
    ext_param: _WS param_expr _WS? "=" _WS? param_expr -> param

    // Parameter expressions (extended keys or traditional/extended values)
    param_expr: param_expr_segment+
    ?param_expr_segment: EXPR_SEGMENT_TEXT -> upper
        | expr_embed
        | STRING -> escaped_str
    EXPR_SEGMENT_TEXT: /[^{"\s=;]+/

    // Filters for semantic noise
    _COMMENT: /\s*;.*/
    _LINE_NUMBER: /N[0-9]+/i
    %import common.WS_INLINE -> _WS

    // Embedded expressions
    ?expr_embed: "{" expr "}"

    // Expressions: Test (called "expr" because it is the lowest precedence
    // expression so it starts the expression rules)
    ?expr: or_expr
        | or_expr "if"i or_expr "else"i expr -> ifelse

    // Expressions: Logical
    ?or_expr: and_expr
        | and_expr "or"i and_expr -> logical_or
    ?and_expr: not_expr
        | not_expr "and"i not_expr -> logical_and
    ?not_expr: "not" not_expr -> logical_not
        | comparison

    // Expressions: Comparisons
    ?comparison: addsub "<" addsub -> less
        | addsub ">" addsub -> greater
        | addsub "<=" addsub -> less_or_equal
        | addsub ">=" addsub -> greater_or_equal
        | addsub "==" addsub -> equal
        | addsub "!=" addsub -> not_equal
        | addsub

    // Expressions: add, subtract, string concat
    ?addsub: multdiv "+" multdiv -> add
        | multdiv "-" multdiv -> subtract
        | multdiv "~" multdiv -> concat
        | multdiv

    // Expression times, divide, modulo
    ?multdiv: factor "*" factor -> multiply
        | factor "/" factor -> divide
        | factor "%" factor -> modulus
        | factor

    // +/- prefix operators
    ?factor: "+" factor -> positive
        | "-" factor -> negative
        | power

    ?power: molecule "**" factor -> power
        | molecule

    ?molecule: molecule "[" expr "]" -> get_member
        | molecule "." IDENTIFIER -> get_member
        | IDENTIFIER "(" (expr ("," expr?)*)? ")" -> call_function
        | atom

    ?atom: "(" expr ")"
        | INT -> int
        | FLOAT -> float
        | STRING -> escaped_str
        | IDENTIFIER -> get_parameter

    // Expression terminals
    %import common.ESCAPED_STRING -> STRING
    %import common.INT
    %import common.FLOAT
    IDENTIFIER: /[A-Z_$]([A-Z0-9_$])?/i
    LTE: "<="
    GTE: ">="
    EQ: "=="
    NE: "!="
    POW: "**"
"""

# Lark defines terminal names using a combination of a private map for single
# characters and terminals defined in the grammar.  There's no good way to
# automatically determine what to display, but we have a finite set.  Sso just
# map manually.
TERMINAL_TO_USER = {
    '$EOL': 'end of line',
    '_WS': 'whitespace',
    'IDENTIFIER': 'IDENTIFIER',
    'DOT': '"."',
    'COMMA': '","',
    'COLON': '":"',
    'SEMICOLON': '";"',
    'PLUS': '"+"',
    'MINUS': '"-"',
    'STAR': '"*"',
    'SLASH': '"/"',
    'BACKSLASH': '"\\"',
    'VBAR': '"|"',
    'QMARK': '"?"',
    'BANG': '"!"',
    'AT': '"@"',
    'HASH': '"#"',
    'DOLLAR': '"$"',
    'PERCENT': '"%"',
    'CIRCUMFLEX': '"^"',
    'AMPERSAND': '"&"',
    'UNDERSCORE': '"_"',
    'LESSTHAN': '"<"',
    'MORETHAN': '">"',
    'EQUAL': '"="',
    'DBLQUOTE': "'\"'",
    'QUOTE': '"\'"',
    'BACKQUOTE': '"`"',
    'TILDE': '"~"',
    'LPAR': '"("',
    'RPAR': '")"',
    'LBRACE': '"{"',
    'RBRACE': '"}"',
    'LSQB': '"["',
    'RSQB': '"]"',
    'LTE': '"<="',
    'GTE': '">="',
    'EQ': '"=="',
    'NE': '"!="',
    'POW': '"**"'
}

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

class InternalError(Exception):
    pass

def _unary_op(op):
    @staticmethod
    @v_args(inline = True)
    def fn(operand):
        return ast.UnaryOp(op = op(), operand = operand)
    return fn

def _make_bin_op(l, op, r):
    return ast.BinOp(left = l, op = op(), right = r)

def _bin_op(op):
    @staticmethod
    @v_args(inline = True)
    def fn(l, r):
        return _make_bin_op(_num_cast(l), op, _num_cast(r))
    return fn

def _make_compare_op(l, op, r):
    return ast.Compare(left = l, ops = [op()], comparators = [ r ])

def _compare(op):
    @staticmethod
    @v_args(inline = True)
    def fn(l, r):
        return _make_compare_op(l, op, r)
    return fn

def _make_bool_op(l, op, r):
    return ast.BoolOp(op = op(), values = [l, r])

def _bool_op(op):
    @staticmethod
    @v_args(inline = True)
    def fn(l, r):
        return _make_bool_op(l, op, r)

def _make_name(name):
    return ast.Name(id = name, ctx = ast.Load)

def _call(name, *args):
    return ast.Call(_make_name(name), args)

def _str(value):
    return _call("str", value)

def _ifelse(test, yes, no):
    return ast.IfExp(test, yes, no)

def _num(value):
    return ast.Num(value)

def _num_cast(value):
    return _call("_runtime_num_cast", value)

def _format_options(options, choice):
    if len(options) > 2:
        for i in range(len(options) - 2):
            options[i] += ","
        options[-2] += " or"
    return ' '.join(options)

def _make_concat(l, r):
    return ast.BinOp(left = l, op = ast.Add(), right = r)

class StatementExprBuilder(Transformer):
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
        return CommandEntry(name, [(ast.Str('*'), arg)] if arg else [])

    @staticmethod
    @v_args(inline = True)
    def trad_command(name, *args):
        return CommandEntry(name, args)

    @staticmethod
    @v_args(inline = True)
    def ext_command(name, *args):
        return CommandEntry(name, args)

    @staticmethod
    @v_args(inline = True)
    def param_expr(*items):
        expr = None
        for item in items:
            if expr:
                expr = _make_concat(expr, item)
            else:
                expr = item
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
    @v_args(inline = True)
    def call_function(name, *args):
        name = str(name).lower()
        if name == "bool" or name == "str" or name == "int" or name == "float":
            return _call(name, *args)
        raise error("Function '{}' is undefined".format(name))

    @staticmethod
    @v_args(inline = True)
    def get_parameter(name):
        name = str(name).lower()
        if name == "inf" or name == "nan":
            return _call("float", _str(name))
        if name == "true":
            return _name("True")
        if name == "false":
            return _name("False")
        return _call("_runtime_get_parameter", name)

    @staticmethod
    def empty_line(*ignored):
        return None

    @staticmethod
    @v_args(inline = True)
    def int(value):
        return _num(int(value))

    @staticmethod
    @v_args(inline = True)
    def float(value):
        return _num(float(value))

    @staticmethod
    @v_args(inline = True)
    def concat(l, r):
        return _make_concat(l, r)

    @staticmethod
    @v_args(inline = True)
    def divide(l, r):
        return _ifelse(
            _make_bin_op(l, ast.Eq, _num(0)),
            _make_bin_op(l, ast.Div, _str(r)),
            _num("nan")
        )

    @staticmethod
    @v_args(inline = True)
    def ifelse(yes, test, no):
        return _ifelse(test, yes, no)

    @staticmethod
    @v_args(inline = True)
    def int_cast(expr):
        return _call("int", _num_cast(expr))

    @staticmethod
    @v_args(inline = True)
    def float_cast(expr):
        return _num_cast(expr)

    @staticmethod
    @v_args(inline = True)
    def get_member(key):
        return _make_call("_runtime_get_member", key)

    logical_not = _unary_op(ast.Not)
    logical_and = _bool_op(ast.And)
    logical_or = _bool_op(ast.Or)
    negative = _unary_op(ast.USub)
    positive = _unary_op(ast.UAdd)
    add = _bin_op(ast.Add)
    subtract = _bin_op(ast.Sub)
    multiply = _bin_op(ast.Mult)
    modulus = _bin_op(ast.Mod)
    power = _bin_op(ast.Pow)
    less = _compare(ast.Lt)
    greater = _compare(ast.Gt)
    less_or_equal = _compare(ast.LtE)
    greater_or_equal = _compare(ast.GtE)
    equal = _compare(ast.Eq)
    not_equal = _compare(ast.NotEq)

#for name in StatementExprBuilder.__dict__:
#    attr = getattr(StatementExprBuilder, name)
#    if (callable(attr)):
#        setattr(StatementExprBuilder, name, staticmethod(v_args(inline = True)(attr)))

class CommandEntry:
    def __init__(self, name, params):
        keys, values = zip(*params) if params else ((), ())
        params = ast.Dict(keys = list(keys), values = list(values))
        expr = ast.Tuple(elts = [ast.Str(str(name).upper()), params], ctx = ast.Load())
        ast.fix_missing_locations(expr)
        try:
            self.code = compile(ast.Expression(expr), '<gcode>', 'eval')
        except Exception as e:
            raise InternalError(
                "{} compiling {}".format(e, ast.dump(expr)))
    def __call__(self, global_scope, local_scope):
        return eval(self.code, global_scope, { "_local": local_scope })

class ErrorEntry:
    COMMAND_PREFIX = "  in command: "
    def __init__(self, exception, line):
        if isinstance(exception, error):
            message = error.message
        if isinstance(exception, UnexpectedCharacters):
            expected = [ self._token_to_user(t) for t in exception.allowed ]
            expected = _format_options(expected, "or")
            message = 'Unexpected "{}"'.format(
                line[exception.column - 1])
            if exception.allowed:
                message = "{} (expected {})".format(message, expected)
        elif isinstance(exception, UnexpectedToken):
            expected = [ self._token_to_user(t) for t in exception.expected ]
            expected = _format_options(expected, "or")
            token = self._token_to_user(exception.token)
            message = "Unexpected {} (expected {})".format(token, expected)
        else:
            message = exception.message
        if hasattr(exception, 'column') and exception.column != '?':
            here = "\n{}^ here".format(
                ' ' * (len(self.COMMAND_PREFIX) + exception.column - 1),
                exception.line)
        else:
            here = ''
        message = "{}\n{}{}{}".format(message, self.COMMAND_PREFIX, line, here)
        self.exception = error(message)
    def __call__(self, global_scope, local_scope):
        raise self.exception
    def _token_to_user(self, token):
        t = token.type if hasattr(token, 'type') else token
        if t in TERMINAL_TO_USER:
            return TERMINAL_TO_USER[t]
        return '"{}"'.format(str(t).lower())

def _runtime_num_cast(possible_num):
    if isinstance(possible_num, Number):
        return possible_num
    try:
        return float(possible_num)
    except exceptions.ValueError:
        return float("nan")

def _lookup_error(message, name, available):
    if not available:
        options = "object is empty"
    elif len(available == 1):
        options = "only available is {}".format(available[0])
    else:
        options = "available are {}".format(_format_options(available,
                                                            "and"))
    raise error(message.format(name, options))

def _runtime_get_member(dict, name):
    if name in dict:
        return dict[name]
    else:
        available = [ "'{}'".format(key) for key in keys(dict) ]
        _lookup_error("No property '{}' ({})", name, options)

def _runtime_get_parameter(name):
    if name in _global:
        return _global[name]
    if name in _local:
        return _local[name]
    else:
        options = keys(_global) + _keys(_local)
        _lookup_error("Parameter '{}' is not defined ({})", name, options)

class PostLex:
    def process(self, stream):
        brace_depth = 0
        for token in stream:
            if token.type == 'LBRACE':
                brace_depth += 1
            elif token.type == 'RBRACE':
                if brace_depth:
                    brace_depth -= 1
            if not brace_depth or token.type != '_WS':
                yield token

    always_accept = ()

class Script:
    """Represents a single block of (possibly unterminated) G-Code input."""

    _parser = Lark(
        GRAMMAR,
        parser = 'lalr',
        transformer = StatementExprBuilder,
        lexer = 'contextual',
        debug = True,
        postlex = PostLex()
    )

    def __init__(self, globals = {}, data = None):
        """Initializes new script, optionally parsing G-Code in data arg."""
        self.entries = deque()
        self._fatal = None
        self._has_m112 = False
        self.globals = {
            "_runtime_num_cast": _runtime_num_cast,
            "_runtime_get_parameter": _runtime_get_parameter,
            "_runtime_get_member": _runtime_get_member,
            "_global": globals
        }
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
                command = self._parser.parse(line)
                if command:
                    if isinstance(command, CommandEntry):
                        self.entries.append(command)
                    else:
                        raise InternalError(
                            "Expected CommandEntry, received {}".format(
                                command))
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
        return entry(self.globals, params)
