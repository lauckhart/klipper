%{
// G-code parser implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_parser.h"
#include "gcode_lexer.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void yyerror(GCodeParser* parser, const char* msg);

struct GCodeParser {
    void* context;
    GCodeLexer* lexer;
    bool in_expr;
    struct yypstate* yyps;
    GCodeNode* statements;
    GCodeNode* last_statement;

    bool (*error)(void*, const char*);
    bool (*statements_fn)(void*, GCodeStatementNode*);
};

static inline GCodeNode* newop2(
    gcode_operator_type_t type,
    GCodeNode* a,
    GCodeNode* b)
{
    GCodeNode* op = gcode_operator_new(type, a);
    gcode_add_next(a, b);
    return op;
}

static inline GCodeNode* newop3(
    gcode_operator_type_t type,
    GCodeNode* a,
    GCodeNode* b,
    GCodeNode* c)
{
    GCodeNode* op = newop2(type, a, b);
    gcode_add_next(b, c);
    return op;
}

static inline void add_statement(GCodeParser* parser, GCodeNode* children) {
    GCodeNode* statement = gcode_statement_new(children);
    if (parser->statements)
        parser->statements = parser->last_statement = statement;
    else {
        gcode_add_next(parser->last_statement,
                       statement);
        parser->last_statement = statement;
    }
}

%}

%define api.pure full
%define api.push-pull push
%define api.token.prefix {TOK_}
%start statements
%param {GCodeParser* parser}

%union {
    int keyword;
    const char* identifier;
    int64_t int_value;
    double float_value;
    const char* str_value;
    GCodeNode* node;
}

%destructor { gcode_node_delete($$); } <node>

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
%token <keyword> MODULUS "%"
%token <keyword> POWER "**"
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
%token <keyword> LPAREN "("
%token <keyword> RPAREN ")"
%token <keyword> NAN "NAN"
%token <keyword> INFINITY "INFINITY"
%token <keyword> TRUE "TRUE"
%token <keyword> FALSE "FALSE"
%token <keyword> LBRACKET "["
%token <keyword> RBRACKET "]"

// This special keyword is an indicator from the lexer that two expressions
// should be concatenated.  Results from G-Code such as X(x)
%token <keyword> BRIDGE "\xff"

%left OR
%left AND
%left EQUAL
%left CONCAT
%left PLUS MINUS
%left TIMES DIVIDE MODULUS
%left LT GT LTE GTE
%right IF ELSE
%left POWER
%precedence NOT
%precedence UNARY
%left DOT

%type <node> statement
%type <node> field
%type <node> expr
%type <node> exprs
%type <node> expr_list
%type <node> string
%type <node> parameter

%%

statements:
  %empty
| statement[s] statements   { if ($s) add_statement(parser, $s); }
;

statement:
  "\n"                      { $$ = NULL; }
| error "\n"                { $$ = NULL; }
| field statement[next]     { $$ = gcode_add_next($field, $next); }
;

field:
  string
| "(" expr ")"              { $$ = $expr; }
| field[a] BRIDGE field[b]  { $$ = newop2(GCODE_CONCAT, $a, $b); }
;

expr:
  "(" expr[e] ")"           { $$ = $e; }
| string
| parameter
| INTEGER                   { $$ = gcode_int_new($1); }
| FLOAT                     { $$ = gcode_float_new($1); }
| TRUE                      { $$ = gcode_bool_new(true); }
| FALSE                     { $$ = gcode_bool_new(false); }
| INFINITY                  { $$ = gcode_float_new(INFINITY); }
| NAN                       { $$ = gcode_float_new(NAN); }
| "!" expr[a]               { $$ = gcode_operator_new(GCODE_NOT, $a); }
| "-" expr[a] %prec UNARY   { $$ = gcode_operator_new(GCODE_NEGATE, $a); }
| "+" expr[a] %prec UNARY   { $$ = $a; }
| expr[a] "+" expr[b]       { $$ = newop2(GCODE_ADD, $a, $b); }
| expr[a] "-" expr[b]       { $$ = newop2(GCODE_SUBTRACT, $a, $b); }
| expr[a] "*" expr[b]       { $$ = newop2(GCODE_MULTIPLY, $a, $b); }
| expr[a] "/" expr[b]       { $$ = newop2(GCODE_DIVIDE, $a, $b); }
| expr[a] MODULUS expr[b]   { $$ = newop2(GCODE_MODULUS, $a, $b); }
| expr[a] POWER expr[b]     { $$ = newop2(GCODE_POWER, $a, $b); }
| expr[a] AND expr[b]       { $$ = newop2(GCODE_AND, $a, $b); }
| expr[a] OR expr[b]        { $$ = newop2(GCODE_OR, $a, $b); }
| expr[a] "<" expr[b]       { $$ = newop2(GCODE_LT, $a, $b); }
| expr[a] ">" expr[b]       { $$ = newop2(GCODE_GT, $a, $b); }
| expr[a] ">=" expr[b]      { $$ = newop2(GCODE_GTE, $a, $b); }
| expr[a] "<=" expr[b]      { $$ = newop2(GCODE_LTE, $a, $b); }
| expr[a] "~" expr[b]       { $$ = newop2(GCODE_CONCAT, $a, $b); }
| expr[a] "=" expr[b]       { $$ = newop2(GCODE_EQUALS, $a, $b); }
| expr[a] "." parameter[b]  { $$ = newop2(GCODE_LOOKUP, $a, $b); }
| expr[a] "[" expr[b] "]"   { $$ = newop2(GCODE_LOOKUP, $a, $b); }
| expr[a] IF expr[b] ELSE expr[c]
                            { $$ = newop3(GCODE_IFELSE, $a, $b, $c); }
| IDENTIFIER[name] "(" exprs[args] ")"
                            { $$ = gcode_function_new($name, $args); }
;

parameter:
  IDENTIFIER                { $$ = gcode_parameter_new($1); }
;

string:
  STRING                    { $$ = gcode_str_new($1); }
 ;

exprs:
  %empty { $$ = NULL; }
| expr_list
;

expr_list:
  expr
| expr[a] "," expr_list[b]  { $$ = $a; gcode_add_next($a, $b); }
;

%%

static bool error(void* context, const char* format, ...) {
    GCodeParser* parser = context;
    va_list argp;
    va_start(argp, format);
    char* buf = malloc(128);
    int rv = vsnprintf(buf, 128, format, argp);
    if (rv < 0)
        parser->error(parser->context, "Internal: Failed to produce error");
    else {
        if (rv > 128) {
            buf = realloc(buf, rv);
            vsnprintf(buf, rv, format, argp);
        }
        parser->error(parser->context, buf);
    }
    free(buf);
    va_end(argp);
    return rv >= 0;
}

#define ERROR(args...) { \
    parser->error(parser->context, args); \
    return false; \
}

#define ASSERT_EXPR { \
    if (!parser->in_expr) \
        ERROR("Internal: Unexpected token type"); \
}

static void yyerror(GCodeParser* parser, const char* msg) {
    error(parser, "G-Code parse error: %s", msg);
}

static bool lex_keyword(void* context, gcode_keyword_t id) {
    GCodeParser* parser = context;
    ASSERT_EXPR;

    yypush_parse(parser->yyps, id, NULL, parser);

    return true;
}

static bool lex_identifier(void* context, const char* name) {
    GCodeParser* parser = context;

    YYSTYPE yys = { .identifier = name };
    yypush_parse(parser->yyps, TOK_IDENTIFIER, &yys, parser);

    return true;
}

static bool lex_string_literal(void* context, const char* value) {
    GCodeParser* parser = context;

    YYSTYPE yys = { .str_value = value };
    yypush_parse(parser->yyps, TOK_STRING, &yys, parser);

    return true;
}

static bool lex_int_literal(void* context, int64_t value) {
    GCodeParser* parser = context;
    ASSERT_EXPR;

    YYSTYPE yys = { .int_value = value };
    yypush_parse(parser->yyps, TOK_INTEGER, &yys, parser);

    return true;
}

static bool lex_float_literal(void* context, double value) {
    GCodeParser* parser = context;
    ASSERT_EXPR;

    YYSTYPE yys = { .float_value = value };
    yypush_parse(parser->yyps, TOK_FLOAT, &yys, parser);

    return true;
}

GCodeParser* gcode_parser_new(
    void* context,
    bool (*error_fn)(void*, const char*),
    bool (*statements_fn)(void*, GCodeStatementNode*))
{
    GCodeParser* parser = malloc(sizeof(GCodeParser));
    if (!parser) {
        error(context, "Out of memory");
        return NULL;
    }

    parser->context = context;
    parser->yyps = NULL;
    parser->in_expr = false;

    parser->error = error_fn;
    parser->statements_fn = statements_fn;

    parser->lexer = gcode_lexer_new(
        parser,
        error,
        lex_keyword,
        lex_identifier,
        lex_string_literal,
        lex_int_literal,
        lex_float_literal
    );
    if (!parser->lexer) {
        free(parser);
        return NULL;
    }

    return parser;
}

bool gcode_parser_parse(GCodeParser* parser, const char* buffer,
                        size_t length)
{
    parser->statements = NULL;
    if (!gcode_lexer_scan(parser->lexer, buffer, length)) {
        gcode_node_delete(parser->statements);
        return false;
    }
    if (parser->statements)
        parser->statements_fn(parser->context,
                              (GCodeStatementNode*)parser->statements);
}

void gcode_parser_finish(GCodeParser* parser) {
    gcode_lexer_finish(parser->lexer);
    if (parser->yyps)
        yypstate_delete(parser->yyps);
    free(parser);
}
