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

static void yyerror(const GCodeLocation* location, GCodeParser* parser,
                    const char* msg);

struct GCodeParser {
    void* context;
    GCodeLexer* lexer;
    bool in_expr;
    struct yypstate* yyps;
    GCodeError* error;
    GCodeLocation location;

    bool (*statement)(void*, GCodeStatementNode*);
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

static inline bool add_statement(GCodeParser* parser, GCodeNode* children) {
    if (!children)
        return true;
    GCodeStatementNode* statement =
        (GCodeStatementNode*)gcode_statement_new(children);
    if (!statement)
        return false;
    parser->statement(parser->context, statement);
    return true;
}

static void out_of_memory(GCodeParser* parser) {
    EMIT_ERROR(parser, "Out of memory (allocating parse node)");
}

#define OOM(val) if (!(val)) { \
    out_of_memory(parser); \
    YYERROR; \
}

typedef GCodeLocation YYLTYPE;
#define YYLTYPE_IS_DECLARED
#define YYLLOC_DEFAULT

static void yyerror(const GCodeLocation* location, GCodeParser* parser,
                    const char* msg)
{
    // Aw Bison...  Our ERROR token is the only way to push an error into the
    // parser, but then we can only skip it using ghetto string matching
    if (strstr(msg, "syntax error, unexpected ERROR"))
        return;
    gcode_error_set_location(parser->error, location);
    EMIT_ERROR(parser, "G-Code parse error: %s", msg);
}

%}

%define api.pure full
%define api.push-pull push
%define api.token.prefix {TOK_}
%define parse.error verbose
%start statements
%param {GCodeParser* parser}
%locations

%union {
    int keyword;
    int64_t int_value;
    double float_value;
    char* str;
    GCodeNode* node;
}

%destructor { free($$); } <str>
%destructor { gcode_node_delete($$); } <node>

%token <str> IDENTIFIER
%token <str> STRING
%token <int_value> INTEGER
%token <float_value> FLOAT

// Pushes lexer errors into the parser
%token <keyword> ERROR

// Indicates two expressions should be concatenated.  Results from G-Code such
// as X{x}
%token <keyword> SPECIAL

// Terminates current statement
%token <keyword> END_OF_STATEMENT

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
%token <keyword> INF "INF"
%token <keyword> TRUE "TRUE"
%token <keyword> FALSE "FALSE"
%token <keyword> LBRACKET "["
%token <keyword> RBRACKET "]"
%token <keyword> LBRACE "{"
%token <keyword> RBRACE "}"

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
%precedence DOT
%precedence LBRACKET
%left BRIDGE

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
| save_statement statements
;

save_statement:
  statement                 { OOM(add_statement(parser, $statement)); }
| error
;

statement:
  END_OF_STATEMENT          { $$ = NULL; }
| field[a] statement[b]     { $$ = gcode_add_next($a, $b); }
;

field:
  string
| "{" expr "}"              { $$ = $expr; }
| field[a] BRIDGE field[b]  { OOM($$ = newop2(GCODE_CONCAT, $a, $b)); }
;

expr:
  "(" expr[e] ")"           { $$ = $e; }
| string
| parameter
| INTEGER                   { OOM($$ = gcode_int_new($1)); }
| FLOAT                     { OOM($$ = gcode_float_new($1)); }
| TRUE                      { OOM($$ = gcode_bool_new(true)); }
| FALSE                     { OOM($$ = gcode_bool_new(false)); }
| INF                       { OOM($$ = gcode_float_new(INFINITY)); }
| NAN                       { OOM($$ = gcode_float_new(NAN)); }
| "!" expr[a]               { OOM($$ = gcode_operator_new(GCODE_NOT, $a)); }
| "-" expr[a] %prec UNARY   { OOM($$ = gcode_operator_new(GCODE_NEGATE, $a)); }
| "+" expr[a] %prec UNARY   { OOM($$ = $a); }
| expr[a] "+" expr[b]       { OOM($$ = newop2(GCODE_ADD, $a, $b)); }
| expr[a] "-" expr[b]       { OOM($$ = newop2(GCODE_SUBTRACT, $a, $b)); }
| expr[a] "*" expr[b]       { OOM($$ = newop2(GCODE_MULTIPLY, $a, $b)); }
| expr[a] "/" expr[b]       { OOM($$ = newop2(GCODE_DIVIDE, $a, $b)); }
| expr[a] MODULUS expr[b]   { OOM($$ = newop2(GCODE_MODULUS, $a, $b)); }
| expr[a] POWER expr[b]     { OOM($$ = newop2(GCODE_POWER, $a, $b)); }
| expr[a] AND expr[b]       { OOM($$ = newop2(GCODE_AND, $a, $b)); }
| expr[a] OR expr[b]        { OOM($$ = newop2(GCODE_OR, $a, $b)); }
| expr[a] "<" expr[b]       { OOM($$ = newop2(GCODE_LT, $a, $b)); }
| expr[a] ">" expr[b]       { OOM($$ = newop2(GCODE_GT, $a, $b)); }
| expr[a] ">=" expr[b]      { OOM($$ = newop2(GCODE_GTE, $a, $b)); }
| expr[a] "<=" expr[b]      { OOM($$ = newop2(GCODE_LTE, $a, $b)); }
| expr[a] "~" expr[b]       { OOM($$ = newop2(GCODE_CONCAT, $a, $b)); }
| expr[a] "=" expr[b]       { OOM($$ = newop2(GCODE_EQUALS, $a, $b)); }
| expr[a] "." parameter[b]  { OOM($$ = newop2(GCODE_LOOKUP, $a, $b)); }
| expr[a] "[" expr[b] "]"   { OOM($$ = newop2(GCODE_LOOKUP, $a, $b)); }
| expr[a] IF expr[b] ELSE expr[c]
                            { OOM($$ = newop3(GCODE_IFELSE, $a, $b, $c)); }
| IDENTIFIER[name] "(" exprs[args] ")"
                            { OOM($$ = gcode_function_new($name, $args)); }
;

parameter:
  IDENTIFIER                { OOM($$ = gcode_parameter_new($1)); free($1); }
;

string:
  STRING                    { OOM($$ = gcode_str_new($1)); free($1); }
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

static void push_error(GCodeParser* parser) {
    // Our lexer generates errors.  With a push parser, the only way to convey
    // the error to Bison is to give it a token with no semantic meaning.  This
    // in turn triggers our error state which skips the remainder of the
    // statement.
    yypush_parse(parser->yyps, TOK_ERROR, NULL, &parser->location, parser);
}

static bool lex_keyword(void* context, gcode_keyword_t id) {
    GCodeParser* parser = context;

    yypush_parse(parser->yyps, id, NULL, &parser->location, parser);

    return true;
}

static bool lex_bridge(void* context) {
    GCodeParser* parser = context;
    yypush_parse(parser->yyps, TOK_BRIDGE, NULL, &parser->location, parser);
    return true;
}

static bool lex_end_of_statement(void* context) {
    GCodeParser* parser = context;
    yypush_parse(parser->yyps, TOK_END_OF_STATEMENT, NULL, &parser->location, parser);
    return true;
}

static inline bool push_string(void* context, int id, const char* value) {
    GCodeParser* parser = context;

    YYSTYPE yys = { .str = strdup(value) };
    if (!yys.str) {
        EMIT_ERROR(parser, "Out of memory (push_string)");
        push_error(parser);
        return false;
    }

    yypush_parse(parser->yyps, id, &yys, &parser->location, parser);

    return true;
}

static bool lex_identifier(void* context, const char* name) {
    push_string(context, TOK_IDENTIFIER, name);
}

static bool lex_string_literal(void* context, const char* value) {
    push_string(context, TOK_STRING, value);
}

static bool lex_int_literal(void* context, int64_t value) {
    GCodeParser* parser = context;

    YYSTYPE yys = { .int_value = value };
    yypush_parse(parser->yyps, TOK_INTEGER, &yys, &parser->location, parser);

    return true;
}

static bool lex_float_literal(void* context, double value) {
    GCodeParser* parser = context;

    YYSTYPE yys = { .float_value = value };
    yypush_parse(parser->yyps, TOK_FLOAT, &yys, &parser->location, parser);

    return true;
}

void lex_error(void* context, const GCodeError* error) {
    GCodeParser* parser = (GCodeParser*)context;
    gcode_error_forward(parser->error, error);
    push_error(parser);
}

GCodeParser* gcode_parser_new(
    void* context,
    void (*error)(void*, const GCodeError* error),
    bool (*statement)(void*, GCodeStatementNode*))
{
    GCodeParser* parser = malloc(sizeof(GCodeParser));
    if (!parser)
        return NULL;

    parser->error = gcode_error_new(context, error);
    if (!error) {
        free(parser);
        return NULL;
    }

    parser->context = context;
    parser->in_expr = false;

    parser->statement = statement;

    parser->lexer = gcode_lexer_new(
        parser,
        &parser->location,
        lex_error,
        lex_keyword,
        lex_identifier,
        lex_string_literal,
        lex_int_literal,
        lex_float_literal,
        lex_bridge,
        lex_end_of_statement
    );
    if (!parser->lexer) {
        gcode_error_delete(parser->error);
        free(parser);
        return NULL;
    }

    parser->yyps = yypstate_new();
    if (!parser->yyps) {
        gcode_error_delete(parser->error);
        gcode_lexer_delete(parser->lexer);
        free(parser);
        return NULL;
    }

    return parser;
}

void gcode_parser_parse(GCodeParser* parser, const char* buffer,
                        size_t length)
{
    gcode_lexer_scan(parser->lexer, buffer, length);
}

void gcode_parser_finish(GCodeParser* parser) {
    gcode_lexer_finish(parser->lexer);
}

void gcode_parser_delete(GCodeParser* parser) {
    gcode_lexer_delete(parser->lexer);
    gcode_error_delete(parser->error);
    if (parser->yyps)
        yypstate_delete(parser->yyps);
    free(parser);
}
