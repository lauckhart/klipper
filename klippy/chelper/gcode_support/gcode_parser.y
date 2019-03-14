%{
// G-code parser implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_lexer.h"

static void yyerror(const char* msg) {
    // TODO
}

%}

%define api.pure full
%define api.push-pull push
%start gcode

%union {
    int keyword;
    const char* identifier;
    int64_t integer_value;
    double float_value;
    const char* string_value;
}

%token <identifier> IDENTIFIER;
%token <int_value> INTEGER;
%token <float_value> FLOAT;
%token <str_value> STRING;

%token <keyword> EOL "\n"
%token <keyword> OR "OR"
%token <keyword> AND "AND"
%token <keyword> EQUAL "="
%token <keyword> CONCAT "~"
%token <keyword> PLUS "+"
%token <keyword> MINUS "-"
%token <keyword> MOD "%"
%token <keyword> TIMES "*"
%token <keyword> DIVIDE "/"
%token <keyword> LT "<"
%token <keyword> GT ">"
%token <keyword> LTE "<="
%token <keyword> GTE ">="
%token <keyword> NOT "!"
%token <keyword> IF "IF"
%token <keyword> ELSE "ELSE"
%token <keyword> DOT "."
%token <keyword> COMMA ","

%left OR
%left AND
%left EQUAL
%left CONCAT
%left PLUS MINUS
%left TIMES DIVIDE MOD
%left LT GT LTE GTE
%precedence NOT
%precedence UNARY
%right IF ELSE

%%

gcode:
  %empty
| line gcode
;

line:
  fields "\n"
;

fields:
  %empty
| field fields;

field:
  sub_expr
| IDENTIFIER;

sub_expr:
  "(" expr ")"
;

expr:
  sub_expr
| parameter
| STRING
| INTEGER
| FLOAT
| "!" expr
| "+" expr %prec UNARY
| "-" expr %prec UNARY
| expr "+" expr
| expr "-" expr
| expr "*" expr
| expr "/" expr
| expr MOD expr
| expr AND expr
| expr OR expr
| expr ">" expr
| expr "<" expr
| expr ">=" expr
| expr "<=" expr
| expr "~" expr
| expr IF expr ELSE expr
| expr "=" expr
| IDENTIFIER "(" exprs ")"
;

parameter:
  IDENTIFIER
| parameter "." IDENTIFIER
;

exprs:
  %empty
| expr_list
;

expr_list:
  expr
| expr "," expr_list
;

%%
