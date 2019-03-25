M18
G1 X=0
G1 X=0 Y=0
M18
G1 X=0
G1 X=0 Y=0
M18
G1 X=0
G1 X=0 Y=0
M18
G1 X=0
G1 X=0 Y=0
M18
G1 x=0
G1 x=0 y=0
M18
G1 x=0
G1 x=0 y=0
M18
G1 x=0
G1 x=0 y=0
M18
G1 x=0
G1 x=0 y=0
M18
G1 X=0
G1 X=0 Y=0
M18
G1 X=0
G1 X=0 Y=0
M18
G1 X=0
G1 X=0 Y=0
M18
G1 X=0
G1 X=0 Y=0
✈
🍺
X1 b=7
X1 a=12 b=cd e=123
X1 A=12 B=cd E=123
X1 a=3 b=7 c=5 d=6
M117 Anything should be allowed here; there aren't even comments
M117 Although expressions are OK
EXTENDED foo=bar biz=baz dee=dum wee=wum
EXTENDED foo=bar biz=baz dee=dum wee=wum
EXTENDED foo=bar biz=baz dee=dum wee=wum
EXTENDED foo=bar biz=baz dee=dum wee=wum
A foo=bar
INT a=0 b=0 c=0 d=0
INT a=10 b=2 c=16 d=8
INT a=1 b=1 c=-1 d=1 e=1 f=10 g=10 h=-10
DEC_F a=0 b=1 c=1.1 d=12.34 e=1 f=-1 g=0 h=0.1 i=0.12
DEC_F a=100 b=1e+10 c=1e-10 d=1.234e+57 e=1.234e-55 f=1e+09 g=1.2e+09
DEC_F h=1.2e-11
HEX_F a=0 b=1 c=1.0625 d=18.2031 e=171.801 f=1
HEX_F h=-1
HEX_F a=1024 b=0.000976562 c=1.31167e+18 d=2.52619e-16
HEX_F a=175924 b=2.38421e-15 c=175924
SPEC_F a=nan b=nan c=nan
SPEC_F a=inf b=inf c=inf
SPEC_F a=-inf b=-inf c=-inf
SPEC_F a=inf b=inf d=inf
BOOL a=true b=true c=true
BOOL a=false b=false c=false
STR a=x b=xy
STR a=foo	bar b=foo\bar
STR a=A b=B
STR a=A b=✈ c=✈
STR a=a b=🍺 c=🍺
BRIDGE a=foobar b=foobar c=foobarbiz d=foobar
BRIDGE a=foobarbizbaz b=foobarbiz
BRIDGE a=foo1 b=1bar c=foo1biz d=12 e=foo12baz f=1bar2
BOOL_OP a=false b=true c=true d=false e=false
BOOL_OP f=true
BOOL_OP a=true b=false c=false
BOOL_OP d=false
BOOL_OP a=true b=true c=true d=false
ARITH_OP a=3 b=3.3 c=3 d=1
ARITH_OP a=-1 b=-1.1 c=-1 d=1
ARITH_OP a=2 b=2.42 c=2 d=0
ARITH_OP a=1 b=0.5 c=0.5 d=nan
ARITH_OP a=0 b=nan c=nan
ARITH_OP a=1 b=1
ARITH_OP a=8 b=13.4895 c=8
INT_COMP a=false b=false c=true
INT_COMP a=false b=true c=true
INT_COMP a=false b=true c=false
INT_COMP a=true b=false c=false
INT_COMP a=true b=true c=false
FLOAT_COMP a=false b=false c=true
FLOAT_COMP a=false b=true c=true
FLOAT_COMP a=false b=true c=false
FLOAT_COMP a=true b=false c=false
FLOAT_COMP a=true b=true c=false
FLOAT_COMP a=true b=false c=false
STR_COMP a=false b=false c=true
STR_COMP a=false b=true c=true
STR_COMP a=false b=true c=false
STR_COMP a=true b=false c=false
STR_COMP a=true b=true c=false
BOOL_COMP a=false b=false c=true
BOOL_COMP a=false b=true c=true
BOOL_COMP a=false b=true c=false
BOOL_COMP a=true b=false c=true
BOOL_COMP a=true b=true c=false
CAST a=3 b=1
CAST a=12 b=1.12.2 c=truefalse
CAST a=false b=true
CAST a=false b=true
CAST a=false b=true c=true
CAST a=false
CAST a=1 b=false c=1 d=1.1
PRECEDENCE a=26
PRECEDENCE b=14
PRECEDENCE c=20
PRECEDENCE d=true
PRECEDENCE e=1
PARAM a=#<dict:foo>
PARAM b=#<dict:bar> c=#<dict:bar>
*** ERROR: Undefined property 'bar'
*** ERROR: Expressions not allowed in command name (line 207:2)
*** ERROR: Expected value after parameter name (line 210:6)
*** ERROR: Expected value after parameter name (line 211:6)
*** ERROR: Expected value after parameter name (line 212:6)
*** ERROR: Expected value after parameter name (line 213:10)
*** ERROR: Expected value after parameter name (line 214:10)
*** ERROR: Expected value after parameter name (line 215:10)
*** ERROR: Expected '=' after parameter name (line 216:14)
*** ERROR: Expected '=' after parameter name (line 217:15)
*** ERROR: Expected '=' after parameter name (line 218:14)
*** ERROR: Expected '=' after parameter name (line 219:18)
*** ERROR: Expected '=' after parameter name (line 220:19)
*** ERROR: Expected '=' after parameter name (line 221:18)
*** ERROR: Expected '=' after parameter name (line 222:15)
*** ERROR: Expected '=' after parameter name (line 223:16)
*** ERROR: Expected '=' after parameter name (line 224:15)
*** ERROR: Undefined parameter 'BAR'
*** ERROR: Undefined property 'bar'
*** ERROR: Undefined property 'BIZ'
*** ERROR: Undefined property 'biz'
*** ERROR: Undefined property '1'
*** ERROR: Undefined property 'BAR'
*** ERROR: Undefined property 'bar'
GOODBYE
