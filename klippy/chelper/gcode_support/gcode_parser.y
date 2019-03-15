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

    bool (*error)(void*, const char*);
    bool (*word)(void*, const char*);
    bool (*expr)(void*, const GCodeNode*);
    bool (*eol)(void*);
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

%}

%define api.pure full
%define api.push-pull push
%define api.token.prefix {TOK_}
%start expr_only
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

%type <node> expr
%type <node> exprs
%type <node> expr_list

%%

// These non-terminals would be useful for parsing entire gcode.  This would be
// required e.g. for functions, if/else, loops, etc.
//
// However, current dialect only requires us to invoke the Bison parser for
// expressions.  We avoid expense of Bison by handling directly in lexer
// callbacks until we encounter an expression.
/*
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
*/

expr_only:
    expr { parser->expr(parser->context, $expr);
           parser->in_expr = false; }
;

expr:
  "(" expr[e] ")"           { $$ = $e; }
| STRING                    { $$ = gcode_str_new($1); }
| INTEGER                   { $$ = gcode_int_new($1); }
| FLOAT                     { $$ = gcode_float_new($1); }
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
| expr[a] "." expr[b]       { $$ = newop2(GCODE_LOOKUP, $a, $b); }
| expr[a] IF expr[b] ELSE expr[c]
                            { $$ = newop3(GCODE_IFELSE, $a, $b, $c); }
| IDENTIFIER[name] "(" exprs[args] ")"
                            { $$ = gcode_function_new($name, $args); }
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
    if (rv > 0) {
        buf = realloc(buf, rv);
        vsnprintf(buf, rv, format, argp);
    }
    parser->error(parser->context, buf);
    free(buf);
    va_end(argp);
    return true;
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

    switch (id) {
        case TOK_LPAREN:
            parser->in_expr = true;
            break;

        case TOK_EOL:
            if (!parser->in_expr)
                return parser->eol(parser->context);
            break;
    }

    ASSERT_EXPR;

    yypush_parse(parser->yyps, id, NULL, parser);

    return true;
}

static bool lex_identifier(void* context, const char* name) {
    GCodeParser* parser = context;

    if (!parser->in_expr)
        return parser->word(parser->context, name);

    YYSTYPE yys = { .identifier = name };
    yypush_parse(parser->yyps, TOK_IDENTIFIER, &yys, parser);

    return true;
}

static bool lex_string_literal(void* context, const char* value) {
    GCodeParser* parser = context;
    ASSERT_EXPR;

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
    bool (*error_fn)(void* context, const char* text),
    bool (*word)(void* context, const char* text),
    bool (*expr)(void* context, const GCodeNode* node),
    bool (*eol)(void* context))
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
    parser->word = word;
    parser->expr = expr;
    parser->eol = eol;

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
    return gcode_lexer_scan(parser->lexer, buffer, length);
}

void gcode_parser_finish(GCodeParser* parser) {
    gcode_lexer_finish(parser->lexer);
    if (parser->yyps)
        yypstate_delete(parser->yyps);
    free(parser);
}
