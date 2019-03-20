; This input file attempts to exhaustively cover G-Code C code


; Lines without statements
; Comment
 ; Comment
N1
 N1
N12
 N12
N1 ; Comment
 N1 ; Comment
N12 ; Comment
 N12 ; Comment
n1
 n1
n12
 n12
n1 ; Comment
 n1 ; Comment
n12 ; Comment
 n12 ; Comment


; Basic instructions

M18
G1 X0
G1 X0 Y0
 M18
 G1 X0
 G1 X0 Y0
N1 M18
N1 G1 X0
N1 G1 X0 Y0
 N1 M18
 N1 G1 X0
 N1 G1 X0 Y0
m18
g1 x0
g1 x0 y0
 m18
 g1 x0
 g1 x0 y0
n1 m18
n1 g1 x0
n1 g1 x0 y0
 n1 m18
 n1 g1 x0
 n1 g1 x0 y0
M18 ; Comment
G1 X0 ; Comment
G1 X0 Y0 ; Comment
 M18 ; Comment
 G1 X0 ; Comment
 G1 X0 Y0 ; Comment
N1 M18 ; Comment
N1 G1 X0 ; Comment
N1 G1 X0 Y0 ; Comment
 N1 M18 ; Comment
 N1 G1 X0 ; Comment
 N1 G1 X0 Y0 ; Comment
✈
🍺


; Single literal expressions

; ints
{0} ; Expr in first field
int {0} {0b0} {00} {0x0}
int {10} {0b10} {0x10} {010}
int {1} {+1} {-1} {++1} {--1} {10} {+10} {-10}

; Decimal floats
dec_f {0.0} {1.0} {1.1} {12.34} {+1.0} {-1.0} {.0} {.1} {.12}
dec_f {1e2} {1e10} {1E-10} {12.34e56} {12.34e-56} {.1e10} {.12e10} {.12e-10}

; Hex floats (who knew?)
hex_f {0x0.0} {0X1.0} {0x1.1} {0x12.34} {0xab.cd} {+0x1.0} {-0x1.0}
hex_f {0x1p10} {0x1P-10} {0x12.34p56} {0x12.34p-56}
hex_f {0xab.cdpef} {0xab.cdp-ef} {0xAb.CdPeF}

; Special floats
spec_f {nan} {NAN} {NaN}
spec_f {inf} {INF} {InF}
spec_f {-inf} {-INF} {-InF}
spec_f {+inf} {+INF} {+InF}

; Booleans
bool {true} {TRUE} {tRuE}
bool {false} {FALSE} {fAlSe}

; Strings
str {"x"} {"xy"}
str {"foo\tbar"} {"foo\\bar"}
str {"\101"} {"\x42"}
str {"\u0041"} {"✈"} {"\u2708"}
str {"\U00000061"} {"🍺"} {"\U0001f37a"}


; Bridge (TODO)

bridge foo{"bar"} {"foo"}bar foo{"bar"}biz {"foo"}{"bar"}
bridge foo{"bar"}{"biz"}baz {"foo"}bar{"biz"}
bridge foo{1} {1}bar foo{1}biz {1}{2} foo{1}{2}baz {1}bar{2}


; Boolean operators

bool_op {not true} {not false} {not 0} {not 1} {not "true"} {not "false"}
bool_op {true and true} {true and false} {false and true} {false and false}
bool_op {true or true} {true or false} {false or true} {false or false}


; Arithmetic operators

arith_op {1 + 2} {1.1 + 2.2} {"1" + "2"} {true + false}
arith_op {1 - 2} {1.1 - 2.2} {"1" - "2"} {true - false}
arith_op {1 * 2} {1.1 * 2.2} {"1" * "2"} {true * false}
arith_op {1 / 1} {1.1 / 2.2} {"1" / "2"} {true / false}
arith_op {false / true} {1 / 0} {1.1 / 0}
arith_op {5 % 2} {"5" % "2"}
arith_op {2 ** 3} {2.2 ** 3.3} {"2" ** "3"}


; Comparison operators

; Integer
int_comp {1 < 0} {1 < 1} {1 < 2}
int_comp {1 <= 0} {1 <= 1} {1 <= 2}
int_comp {1 == 0} {1 == 1} {1 == 2}
int_comp {1 > 0} {1 > 1} {1 > 2}
int_comp {1 >= 0} {1 >= 1} {1 >= 2}

; Float
float_comp {1.1 < 0.1} {1.1 < 1.1} {1.1 < 2.1}
float_comp {1.1 <= 0.1} {1.1 <= 1.1} {1.1 <= 2.1}
float_comp {1.1 == 0.1} {1.1 == 1.1} {1.1 == 2.1}
float_comp {1.1 > 0.1} {1.1 > 1.1} {1.1 > 2.1}
float_comp {1.1 >= 0.1} {1.1 >= 1.1} {1.0 >= 2.1}
float_comp {inf == inf} {-inf == inf} {nan == nan}

; String
str_comp {"b" < "a"} {"b" < "b"} {"b" < "c"}
str_comp {"b" <= "a"} {"b" <= "b"} {"b" <= "c"}
str_comp {"b" == "a"} {"b" == "b"} {"b" == "c"}
str_comp {"b" > "a"} {"b" > "b"} {"b" > "c"}
str_comp {"b" >= "a"} {"b" >= "b"} {"b" >= "c"}

; Boolean
bool_comp {true < false} {true < true} {false < true}
bool_comp {true <= false} {true <= true} {false <= true}
bool_comp {true == false} {true == true} {false == true}
bool_comp {true > false} {true < true} {false < true}
bool_comp {true >= false} {true >= true} {false >= true}


; Casts

cast {"1" + "2"} {true + false}
cast {1 ~ 2} {1.1 ~ 2.2} {true ~ false}
cast {"false" or "false"} {"false" or "true"}
cast {0 or 0} {0 or 1}
cast {"0" or "0"} {"0" or "1"} {"foo" or "bar"}
cast {1 == 2.1}
cast {str(1)} {boolean("false")} {int("01")} {float("1.1")}


; Precedence

precedence {2 + 4 * 6}
precedence {2 * 4 + 6}
precedence {2 * (4 + 6)}
precedence {not false == true}
precedence {-1 + 2}


; Parameter references, lookup & dict serialization

param {foo}
param {foo.bar} {foo["BAR"]}
param {foo.bar.biz} {foo["bar"]["biz"]}
param {bar} ; Error
param {foo["bar"]} ; Error
param {foo.biz} ; Error
param {foo["biz"]} ; Error
param {foo[1]} ; Error
param {"foo".bar} ; Error
param {"foo"["bar"]} ; Error


; Lexer errors (TODO)


; Parser errors (TODO)


; Interpreter errors (TODO)


; No newline

goodbye