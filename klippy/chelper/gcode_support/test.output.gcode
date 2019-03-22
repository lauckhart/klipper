M18
G1 X0
G1 X0 Y0
M18
G1 X0
G1 X0 Y0
M18
G1 X0
G1 X0 Y0
M18
G1 X0
G1 X0 Y0
M18
G1 x0
G1 x0 y0
M18
G1 x0
G1 x0 y0
M18
G1 x0
G1 x0 y0
M18
G1 x0
G1 x0 y0
M18
G1 X0
G1 X0 Y0
M18
G1 X0
G1 X0 Y0
M18
G1 X0
G1 X0 Y0
M18
G1 X0
G1 X0 Y0
✈
🍺
INT 0 0 0 0
INT 10 2 16 8
INT 1 1 -1 1 1 10 10 -10
DEC_F 0 1 1.1 12.34 1 -1 0 0.1 0.12
DEC_F 100 1e+10 1e-10 1.234e+57 1.234e-55 1e+09 1.2e+09 1.2e-11
HEX_F 0 1 1.0625 18.2031 171.801 1 -1
HEX_F 1024 0.000976562 1.31167e+18 2.52619e-16
HEX_F 175924 2.38421e-15 175924
SPEC_F nan nan nan
SPEC_F inf inf inf
SPEC_F -inf -inf -inf
SPEC_F inf inf inf
BOOL true true true
BOOL false false false
STR x xy
STR foo	bar foo\bar
STR A B
STR A ✈ ✈
STR a 🍺 🍺
BRIDGE foobar foobar foobarbiz foobar
BRIDGE foobarbizbaz foobarbiz
BRIDGE foo1 1bar foo1biz 12 foo12baz 1bar2
BOOL_OP false true true false false true
BOOL_OP true false false false
BOOL_OP true true true false
ARITH_OP 3 3.3 3 1
ARITH_OP -1 -1.1 -1 1
ARITH_OP 2 2.42 2 0
ARITH_OP 1 0.5 0.5 nan
ARITH_OP 0 nan nan
ARITH_OP 1 1
ARITH_OP 8 13.4895 8
INT_COMP false false true
INT_COMP false true true
INT_COMP false true false
INT_COMP true false false
INT_COMP true true false
FLOAT_COMP false false true
FLOAT_COMP false true true
FLOAT_COMP false true false
FLOAT_COMP true false false
FLOAT_COMP true true false
FLOAT_COMP true false false
STR_COMP false false true
STR_COMP false true true
STR_COMP false true false
STR_COMP true false false
STR_COMP true true false
BOOL_COMP false false true
BOOL_COMP false true true
BOOL_COMP false true false
BOOL_COMP true false true
BOOL_COMP true true false
CAST 3 1
CAST 12 1.12.2 truefalse
CAST false true
CAST false true
CAST false true true
CAST false
CAST 1 false 1 1.1
PRECEDENCE 26
PRECEDENCE 14
PRECEDENCE 20
PRECEDENCE true
PRECEDENCE 1
PARAM #<dict:foo>
PARAM #<dict:bar> #<dict:bar>
*** ERROR: Undefined property 'bar'
*** ERROR: Undefined parameter 'BAR'
*** ERROR: Undefined property 'bar'
*** ERROR: Undefined property 'BIZ'
*** ERROR: Undefined property 'biz'
*** ERROR: Undefined property '1'
*** ERROR: Undefined property 'BAR'
*** ERROR: Undefined property 'bar'
*** ERROR: Expressions not allowed in command name (line 193:3)
GOODBYE
