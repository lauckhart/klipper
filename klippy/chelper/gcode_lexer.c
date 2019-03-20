// G-code lexer implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_lexer.h"
#include <stdlib.h>
#include <math.h>

#define ENTER_EXPR '{'
#define EXIT_EXPR '}'

#define TOKEN_STOP { \
    if (lexer->location) { \
        lexer->location->last_line = lexer->line; \
        lexer->location->last_column = lexer->column + 1; \
    } \
}

#define TOKEN_START { \
    if (lexer->location) { \
        lexer->location->first_line = lexer->line; \
        lexer->location->first_column = lexer->column; \
    } \
    TOKEN_STOP; \
}

#define ERROR(args...) { \
    gcode_error_set_location(lexer->error, lexer->location); \
    EMIT_ERROR(lexer, args); \
    lexer->state = SCAN_ERROR; \
}

// These are defined in gcode_parser.keywords.c
struct GCodeKeywordDetail {
    char *name;
    int id;
};
typedef struct GCodeKeywordDetail GCodeKeywordDetail;
GCodeKeywordDetail* gcode_keyword_lookup(register const char *str,
                                         register size_t len);

typedef enum state_t {
    SCAN_NEWLINE,
    SCAN_ERROR,
    SCAN_LINENO,
    SCAN_AFTER_LINENO,
    SCAN_STATEMENT,
    SCAN_WORD,
    SCAN_COMMENT,
    SCAN_EMPTY_LINE_COMMENT,
    SCAN_EXPR,
    SCAN_AFTER_EXPR,
    SCAN_SYMBOL,
    SCAN_IDENTIFIER,
    SCAN_STR,
    SCAN_STR_ESCAPE,
    SCAN_STR_OCTAL,
    SCAN_STR_HEX,
    SCAN_STR_LOW_UNICODE,
    SCAN_STR_HIGH_UNICODE,
    SCAN_NUMBER_BASE,
    SCAN_DECIMAL,
    SCAN_HEX,
    SCAN_BINARY,
    SCAN_OCTAL,
    SCAN_DECIMAL_FLOAT,
    SCAN_DECIMAL_FRACTION,
    SCAN_DECIMAL_EXPONENT_SIGN,
    SCAN_DECIMAL_EXPONENT,
    SCAN_HEX_FLOAT,
    SCAN_HEX_FRACTION,
    SCAN_HEX_EXPONENT_SIGN,
    SCAN_HEX_EXPONENT
} state_t;

struct GCodeLexer {
    bool (*keyword)(void*, gcode_keyword_t id);
    bool (*identifier)(void*, const char* value);
    bool (*str_literal)(void*, const char* value);
    bool (*int_literal)(void*, int64_t value);
    bool (*float_literal)(void*, double value);
    bool (*bridge)(void*);
    bool (*end_of_statement)(void*);

    void*   context;
    state_t state;
    char*   token_str;
    size_t  token_length;
    size_t  token_limit;
    size_t  expr_nesting;
    int64_t int_value;
    double  float_value;
    int8_t  exponent_sign;
    int8_t  digit_count;
    double  float_fraction_multiplier;
    uint32_t line;
    uint32_t column;
    GCodeLocation* location;
    GCodeError* error;
};

GCodeLexer* gcode_lexer_new(
    void* context,
    GCodeLocation* location,
    void (*error)(void*, const GCodeError* error),
    bool (*keyword)(void*, gcode_keyword_t id),
    bool (*identifier)(void*, const char* value),
    bool (*str_literal)(void*, const char* value),
    bool (*int_literal)(void*, int64_t value),
    bool (*float_literal)(void*, double value),
    bool (*bridge)(void*),
    bool (*end_of_statement)(void*)
) {
    GCodeLexer* lexer = malloc(sizeof(GCodeLexer));
    if (!lexer)
        return NULL;

    lexer->error = gcode_error_new(context, error);
    if (!lexer->error) {
        free(lexer);
        return NULL;
    }

    lexer->context = context;
    lexer->location = location;

    lexer->keyword = keyword;
    lexer->identifier = identifier;
    lexer->str_literal = str_literal;
    lexer->int_literal = int_literal;
    lexer->float_literal = float_literal;
    lexer->bridge = bridge;
    lexer->end_of_statement = end_of_statement;

    lexer->expr_nesting = 0;
    lexer->token_str = NULL;
    lexer->token_length = lexer->token_limit = 0;

    gcode_lexer_reset(lexer);

    return lexer;
}

static bool gcode_token_alloc(GCodeLexer* lexer) {
    if (lexer->token_str == NULL) {
        lexer->token_limit = 128;
        lexer->token_str = malloc(lexer->token_limit);
        if (!lexer->token_str) {
            TOKEN_STOP;
            ERROR("Out of memory (gcode_token_alloc)");
            return false;
        }
    } else {
        lexer->token_limit *= 2;
        lexer->token_str = realloc(lexer->token_str, lexer->token_limit);
        if (!lexer->token_str) {
            TOKEN_STOP;
            ERROR("Out of memory (gcode_token_alloc)");
            return false;
        }
    }
    return true;
}

static inline bool token_char(GCodeLexer* lexer, const char ch) {
    if (lexer->token_length == lexer->token_limit
        && !gcode_token_alloc(lexer)
    )
        return false;
    lexer->token_str[lexer->token_length++] = ch;
    return true;
}

static inline bool add_str_wchar(GCodeLexer* lexer) {
    char buf[MB_CUR_MAX];

    int len = wctomb(buf, lexer->int_value);
    if (len == -1) {
        token_char(lexer, '?');
        return true;
    }

    for (int i = 0; i < len; i++)
        if (!token_char(lexer, buf[i]))
            return false;
    return true;
}

static inline bool terminate_token(GCodeLexer* lexer) {
    return token_char(lexer, '\0');
}

static inline char hex_digit_to_int(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'Z')
        return 10 + ch - 'A';
    return -1;
}

static inline void add_safe_digit(GCodeLexer* lexer, int8_t value,
                           int8_t base)
 {
    lexer->int_value = lexer->int_value * base + value;
    lexer->digit_count++;
}

static inline void set_exponent(GCodeLexer* lexer, int8_t base) {
    lexer->float_value *=
        pow(base, lexer->exponent_sign * lexer->int_value);
}

static inline void add_float_digit(GCodeLexer* lexer, int8_t value,
                            int8_t base)
{
    lexer->float_value = lexer->float_value * base + value;
}

static inline void add_float_fraction_digit(GCodeLexer* lexer, int8_t value,
                                     int8_t base)
{
    lexer->float_fraction_multiplier /= base;
    lexer->float_value += value * lexer->float_fraction_multiplier;
}

static inline bool is_ident_char(char ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9')
        || ch == '_'
        || ch == '$';
}

static inline bool is_symbol_char(char ch) {
    switch (ch) {
        case '`':
        case '~':
        case '!':
        case '@':
        case '#':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '+':
        case '=':
        case '{':
        case '[':
        case '}':
        case ']':
        case '|':
        case '\\':
        case ':':
        case ',':
        case '<':
        case '.':
        case '>':
        case '?':
        case '/':
            return true;
    }

    return false;
}

static inline int get_keyword_id(GCodeLexer* lexer) {
    GCodeKeywordDetail* detail = gcode_keyword_lookup(lexer->token_str,
                                                      lexer->token_length - 1);
    if (detail)
        return detail->id;
    return -1;
}

static inline bool free_token(GCodeLexer* lexer) {
    lexer->token_length = 0;
}

#define GET_KEYWORD_ID \
    if (!terminate_token(lexer)) { \
        free_token(lexer); \
        return false; \
    } \
    int id = get_keyword_id(lexer);

static inline bool emit_symbol(GCodeLexer* lexer) {
    TOKEN_STOP;
    GET_KEYWORD_ID;

    if (id == -1) {
        ERROR("Illegal operator '%s'", lexer->token_str);
        free_token(lexer);
        return false;
    }
    free_token(lexer);

    if (!lexer->keyword(lexer->context, id)) {
        lexer->state = SCAN_ERROR;
        return false;
    }

    return true;
}

static inline bool emit_char_symbol(GCodeLexer* lexer, char ch) {
    TOKEN_START;
    TOKEN_STOP;
    token_char(lexer, ch);
    GET_KEYWORD_ID;
    free_token(lexer);

    if (id == -1) {
        ERROR("Internal: Attempt to emit unknown symbol '%c'", ch);
        return false;
    }

    if (!lexer->keyword(lexer->context, id)) {
        lexer->state = SCAN_ERROR;
        return false;
    }

    return true;
}

static inline bool emit_bridge(GCodeLexer* lexer) {
    TOKEN_START;
    TOKEN_STOP;
    if (!lexer->bridge(lexer->context)) {
        lexer->state = SCAN_ERROR;
        return false;
    }
    return true;
}

static inline bool emit_end_of_statement(GCodeLexer* lexer) {
    TOKEN_START;
    TOKEN_STOP;
    lexer->end_of_statement(lexer->context);
    lexer->state = SCAN_NEWLINE;
    return true;
}

#define EMIT_CHAR_SYMBOL() emit_char_symbol(lexer, ch)

static inline bool emit_str(GCodeLexer* lexer) {
    TOKEN_STOP;
    if (!terminate_token(lexer)) {
        free_token(lexer);
        return false;
    }
    if (!lexer->str_literal(lexer->context, lexer->token_str)) {
        lexer->state = SCAN_ERROR;
        free_token(lexer);
        return false;
    }
    free_token(lexer);
    return true;
}

static inline bool emit_int(GCodeLexer* lexer, int value) {
    TOKEN_STOP;
    if (!lexer->int_literal(lexer->context, value)) {
        lexer->state = SCAN_ERROR;
        return false;
    }
    return true;
}

#define EMIT_INT() emit_int(lexer, lexer->int_value)

static inline bool emit_float(GCodeLexer* lexer, double value) {
    TOKEN_STOP;
    if (!lexer->float_literal(lexer->context, value)) {
        lexer->state = SCAN_ERROR;
        return false;
    }
    return true;
}

#define EMIT_FLOAT() emit_float(lexer, lexer->float_value)

static inline bool emit_keyword_or_identifier(GCodeLexer* lexer) {
    TOKEN_STOP;
    GET_KEYWORD_ID;

    bool result;
    if (id == -1)
        result = lexer->identifier(lexer->context, lexer->token_str);
    else
        result = lexer->keyword(lexer->context, id);

    free_token(lexer);
    return result;
}

static inline bool digit_exceeds(GCodeLexer* lexer, uint8_t value,
                                 int16_t base, int64_t max)
{
    return lexer->int_value > (max - value) / base;
}

static inline bool add_digit(GCodeLexer* lexer, uint8_t value, int16_t base,
    int64_t max, const char* err)
{
    if (digit_exceeds(lexer, value, base, max)) {
        ERROR(err);
        free_token(lexer);
        return false;
    }
    add_safe_digit(lexer, value, base);
    return true;
}

#define TOKEN_CHAR(ch) token_char(lexer, ch)

#define TOKEN_CHAR_UPPER() \
    TOKEN_CHAR(ch >= 'a' && ch <= 'z' ? ch - 32 : ch)

#define CASE_SPACE case ' ': case '\t': case '\v': case '\r':

#define BACK_UP { \
    buffer--; \
    if (ch == '\n') \
        lexer->line--; \
}

#define CASE_STR_ESC(esc_ch, ch) \
    case esc_ch: \
        if (TOKEN_CHAR(ch)) \
            lexer->state = SCAN_STR; \
        break;

static const int UNICODE_MAX = 0x10ffff;

// Get ready for monster switch statement.  Two reasons for this:
//   - Performance (no function call overhead)
//   - Incremental scanning (buffer may terminate anywhere in a statement)
void gcode_lexer_scan(GCodeLexer* lexer, const char* buffer, size_t length) {
    const char* end = buffer + length;
    int8_t digit_value;
    for (char ch = *buffer; buffer < end; ch = (*++buffer)) {
        if (ch == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else
            lexer->column++;
        switch (lexer->state) {
        case SCAN_NEWLINE:
            switch (ch) {
                case 'N':
                case 'n':
                    lexer->state = SCAN_LINENO;
                    break;

                case ';':
                    lexer->state = SCAN_EMPTY_LINE_COMMENT;
                    break;

                case '\n':
                CASE_SPACE
                    break;

                default:
                    BACK_UP;
                    lexer->state = SCAN_STATEMENT;
                    break;
            }
            break;

        case SCAN_ERROR:
            if (ch == '\n')
                lexer->state = SCAN_NEWLINE;
            break;

        case SCAN_LINENO:
            switch (ch) {
                case '\n':
                    lexer->state = SCAN_NEWLINE;
                    break;

                CASE_SPACE
                    lexer->state = SCAN_AFTER_LINENO;
                    break;

                case ';':
                    lexer->state = SCAN_EMPTY_LINE_COMMENT;
                    break;

                case ENTER_EXPR:
                    if (EMIT_CHAR_SYMBOL())
                        lexer->state = SCAN_EXPR;
                    break;
            }
            break;

        case SCAN_AFTER_LINENO:
            switch (ch) {
                case '\n':
                    lexer->state = SCAN_NEWLINE;
                    break;

                CASE_SPACE
                    break;

                case ';':
                    lexer->state = SCAN_EMPTY_LINE_COMMENT;
                    break;

                default:
                    BACK_UP;
                    lexer->state = SCAN_STATEMENT;
                    break;
            }
            break;
            
        case SCAN_STATEMENT:
            switch (ch) {
                case ENTER_EXPR:
                    if (EMIT_CHAR_SYMBOL()) {
                        lexer->state = SCAN_EXPR;
                        lexer->expr_nesting = 0;
                    }
                    break;

                case '\n':
                    emit_end_of_statement(lexer);
                    break;

                case ';':
                    lexer->state = SCAN_COMMENT;
                    break;

                CASE_SPACE
                    break;

                default:
                    TOKEN_START;
                    lexer->state = SCAN_WORD;
                    BACK_UP;
                    break;
            }
            break;

        case SCAN_WORD:
            switch (ch) {
                case '\n':
                    emit_str(lexer);
                    emit_end_of_statement(lexer);
                    break;

                CASE_SPACE
                    if (emit_str(lexer))
                        lexer->state = SCAN_STATEMENT;
                    break;

                case ';':
                    if (emit_str(lexer))
                        lexer->state = SCAN_COMMENT;
                    break;

                case ENTER_EXPR:
                    if (emit_str(lexer) && emit_bridge(lexer)
                        && EMIT_CHAR_SYMBOL())
                    {
                        lexer->state = SCAN_EXPR;
                        lexer->expr_nesting = 0;
                    }
                    break;

                default:
                    TOKEN_CHAR_UPPER();
                    break;
            }
            break;

        case SCAN_COMMENT:
            if (ch == '\n')
                emit_end_of_statement(lexer);
            break;

        case SCAN_EMPTY_LINE_COMMENT:
            if (ch == '\n')
                lexer->state = SCAN_NEWLINE;
            break;

        case SCAN_EXPR:
            switch (ch) {
                case '\n':
                    TOKEN_START;
                    TOKEN_STOP;
                    ERROR("Unterminated expression");
                    lexer->state = SCAN_NEWLINE;
                    break;

                CASE_SPACE
                    break;

                case '(':
                    lexer->expr_nesting++;
                    EMIT_CHAR_SYMBOL();
                    break;

                case ')':
                    if (lexer->expr_nesting)
                        lexer->expr_nesting--;
                    EMIT_CHAR_SYMBOL();
                    break;

                case EXIT_EXPR:
                    if (EMIT_CHAR_SYMBOL())
                        lexer->state = SCAN_AFTER_EXPR;
                    break;

                case '0':
                    TOKEN_START;
                    lexer->state = SCAN_NUMBER_BASE;
                    break;

                case '\'':
                case '`':
                    TOKEN_START;
                    ERROR("Unexpected character %c", ch);
                    break;

                case '.':
                    TOKEN_START;
                    lexer->float_value = 0;
                    lexer->float_fraction_multiplier = 1;
                    lexer->state = SCAN_DECIMAL_FRACTION;
                    break;

                case '"':
                    TOKEN_START;
                    lexer->state = SCAN_STR;
                    break;

                default:
                    TOKEN_START;
                    if (ch >= '1' && ch <= '9') {
                        lexer->int_value = 0;
                        lexer->digit_count = 0;
                        lexer->state = SCAN_DECIMAL;
                        BACK_UP;
                    } else if (is_symbol_char(ch)) {
                        lexer->state = SCAN_SYMBOL;
                        TOKEN_CHAR(ch);
                    } else {
                        lexer->state = SCAN_IDENTIFIER;
                        TOKEN_CHAR_UPPER();
                    }
                    break;
            }
            break;

        case SCAN_AFTER_EXPR:
            switch (ch) {
                case '\n':
                case ';':
                CASE_SPACE
                    lexer->state = SCAN_STATEMENT;
                    break;

                default:
                    if (emit_bridge(lexer))
                        lexer->state = SCAN_WORD;
                    break;
            }
            BACK_UP;
            break;

        case SCAN_SYMBOL:
            if (is_symbol_char(ch))
                TOKEN_CHAR(ch);
            else {
                if (!emit_symbol(lexer))
                    break;
                if (ch == '\n') {
                    TOKEN_START;
                    ERROR("Unterminated expression");
                    lexer->state = SCAN_NEWLINE;
                } else {
                    lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
            }
            break;

        case SCAN_IDENTIFIER:
            if (is_ident_char(ch))
                TOKEN_CHAR_UPPER();
            else {
                if (!emit_keyword_or_identifier(lexer))
                    break;
                lexer->state = SCAN_EXPR;
                BACK_UP;
            }
            break;

        case SCAN_STR:
            switch (ch) {
                case '\\':
                    lexer->state = SCAN_STR_ESCAPE;
                    break;

                case '"':
                    if (emit_str(lexer))
                        lexer->state = SCAN_EXPR;
                    break;

                case '\n':
                    ERROR("Unterminated string");
                    free_token(lexer);
                    lexer->state = SCAN_NEWLINE;
                    break;

                default:
                    TOKEN_CHAR(ch);
                    break;
            }
            break;

        case SCAN_STR_ESCAPE:
            switch (ch) {
            CASE_STR_ESC('a', 0x07);
            CASE_STR_ESC('b', 0x08);
            CASE_STR_ESC('e', 0x1b);
            CASE_STR_ESC('f', 0x0c);
            CASE_STR_ESC('n', 0x0a);
            CASE_STR_ESC('r', 0x0d);
            CASE_STR_ESC('t', 0x09);
            CASE_STR_ESC('v', 0x0b);
            CASE_STR_ESC('\\', 0x5c);
            CASE_STR_ESC('\'', 0x27);
            CASE_STR_ESC('"', 0x22);
            CASE_STR_ESC('?', 0x3f);

            case 'x':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_STR_HEX;
                break;

            case 'u':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_STR_LOW_UNICODE;
                break;

            case 'U':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_STR_HIGH_UNICODE;
                break;

            case '\n':
                ERROR("Unterminated string");
                free_token(lexer);
                lexer->state = SCAN_NEWLINE;
                break;

            default:
                if (ch >= '0' && ch <= '9') {
                    lexer->int_value = 0;
                    lexer->digit_count = 0;
                    lexer->state = SCAN_STR_OCTAL;
                    BACK_UP;
                } else {
                    ERROR("Illegal string escape \\%c", ch);
                    free_token(lexer);
                }
                break;
            }
            break;

        case SCAN_STR_OCTAL:
            if (ch >= '0' && ch <= '7') {
                if (add_digit(lexer, ch - '0', 8, 255,
                              "Octal escape (\\nnn) exceeds byte value")
                    && lexer->digit_count == 3
                    && TOKEN_CHAR(lexer->int_value)
                )
                    lexer->state = SCAN_STR;
            } else if (ch == '8' || ch == '9') {
                ERROR("Illegal digit in octal escape (\\nnn)");
                free_token(lexer);
            } else {
                if (TOKEN_CHAR(lexer->int_value))
                    lexer->state = SCAN_STR;
                BACK_UP;
            }
            break;

        case SCAN_STR_HEX:
            digit_value = hex_digit_to_int(ch);
            if (digit_value == -1) {
                if (!lexer->digit_count) {
                    ERROR("Hex string escape (\\x) requires at least one "
                          "digit");
                    free_token(lexer);
                    break;
                }
                if (TOKEN_CHAR(lexer->int_value))
                    lexer->state = SCAN_STR;
                BACK_UP;
            }else if (!add_digit(lexer, digit_value, 16, 255,
                                 "Hex escape exceeds byte value"))
                lexer->state = SCAN_ERROR;
            break;

        case SCAN_STR_LOW_UNICODE:
            digit_value = hex_digit_to_int(ch);
            if (digit_value == -1) {
                ERROR("Low unicode escape (\\u) requires exactly four "
                      "digits");
                free_token(lexer);
                break;
            }
            add_safe_digit(lexer, digit_value, 16);
            if (lexer->digit_count == 4
                && add_str_wchar(lexer)
            )
                lexer->state = SCAN_STR;
            break;

        case SCAN_STR_HIGH_UNICODE:
            digit_value = hex_digit_to_int(ch);
            if (digit_value == -1) {
                ERROR("High unicode escape (\\U) requires exactly eight "
                      "digits");
                free_token(lexer);
                break;
            }
            if (add_digit(lexer, digit_value, 16, UNICODE_MAX,
                      "High unicode escape (\\U) exceeds unicode value")
                && lexer->digit_count == 8
                && add_str_wchar(lexer)
            )
                lexer->state = SCAN_STR;
            break;

        case SCAN_NUMBER_BASE:
            switch (ch) {
            case 'b':
            case 'B':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_BINARY;
                break;

            case 'x':
            case 'X':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_HEX;
                break;

            case '.':
                lexer->float_value = 0;
                lexer->float_fraction_multiplier = 1;
                lexer->state = SCAN_DECIMAL_FRACTION;
                break;

            case 'e':
            case 'E':
                lexer->float_value = 0;
                lexer->state = SCAN_DECIMAL_EXPONENT_SIGN;
                break;

            default:
                if (ch >= '0' && ch <= '9') {
                    lexer->int_value = 0;
                    lexer->state = SCAN_OCTAL;
                    BACK_UP;
                } else {
                    if (emit_int(lexer, 0))
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_DECIMAL:
            switch (ch) {
            case '.':
                lexer->float_value = lexer->int_value;
                lexer->float_fraction_multiplier = 1;
                lexer->state = SCAN_DECIMAL_FRACTION;
                break;

            case 'e':
            case 'E':
                lexer->float_value = lexer->int_value;
                lexer->state = SCAN_DECIMAL_EXPONENT_SIGN;
                break;

            default:
                if (ch >= '0' && ch <= '9') {
                    if (digit_exceeds(lexer, ch - '0', 10, INT64_MAX)) {
                        lexer->float_value = lexer->int_value;
                        lexer->state = SCAN_DECIMAL_FLOAT;
                    } else
                        add_safe_digit(lexer, ch - '0', 10);
                } else {
                    if (emit_int(lexer, lexer->int_value))
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_HEX:
            switch (ch) {
            case '.':
                lexer->float_value = lexer->int_value;
                lexer->float_fraction_multiplier = 1;
                lexer->state = SCAN_HEX_FRACTION;
                break;

            case 'p':
            case 'P':
                lexer->float_value = lexer->int_value;
                lexer->state = SCAN_HEX_EXPONENT_SIGN;
                break;

            default:
                digit_value = hex_digit_to_int(ch);
                if (digit_value != -1) {
                    if (digit_exceeds(lexer, digit_value, 16, INT64_MAX)) {
                        lexer->float_value = lexer->int_value;
                        lexer->state = SCAN_HEX_FLOAT;
                    } else
                        add_safe_digit(lexer, digit_value, 16);
                } else {
                    if (EMIT_INT())
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_BINARY:
            if (ch == '0' || ch == '1') {
                add_digit(lexer, ch - '0', 2, INT64_MAX,
                    "Binary literal exceeds maximum value");
            } else if (ch == '.') {
                ERROR("Fractional binary literals not allowed");
            } else if (ch >= '2' && ch <= '9') {
                ERROR("Illegal binary digit %c", ch);
            } else {
                if (EMIT_INT())
                    lexer->state = SCAN_EXPR;
                BACK_UP;
            }
            break;

        case SCAN_OCTAL:
            if (ch >= '0' && ch <= '7') {
                add_digit(lexer, ch - '0', 8, INT64_MAX,
                    "Octal literal exceeds maximum value");
            } else if (ch == '.') {
                ERROR("Fractional octal literals not allowed");
            } else if (ch == '8' || ch == '9') {
                ERROR("Illegal octal digit %c", ch);
            } else {
                if (EMIT_INT())
                    lexer->state = SCAN_EXPR;
                BACK_UP;
            }
            break;

        case SCAN_DECIMAL_FLOAT:
            switch (ch) {
            case '.':
                lexer->float_fraction_multiplier = 1;
                lexer->state = SCAN_DECIMAL_FRACTION;
                break;

            case 'e':
            case 'E':
                lexer->state = SCAN_DECIMAL_EXPONENT_SIGN;
                break;

            default:
                if (ch >= '0' && ch <= '9')
                    add_float_digit(lexer, ch - '0', 10);
                else {
                    if (EMIT_FLOAT())
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_DECIMAL_FRACTION:
            switch (ch) {
            case 'e':
            case 'E':
                lexer->state = SCAN_DECIMAL_EXPONENT_SIGN;
                break;

            default:
                if (ch >= '0' && ch <= '9')
                    add_float_fraction_digit(lexer, ch - '0', 10);
                else {
                    if (EMIT_FLOAT())
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_DECIMAL_EXPONENT_SIGN:
            if (ch == '-')
                lexer->exponent_sign = -1;
            else {
                lexer->exponent_sign = 1;
                BACK_UP;
            }
            lexer->int_value = 0;
            lexer->digit_count = 0;
            lexer->state = SCAN_DECIMAL_EXPONENT;
            break;

        case SCAN_DECIMAL_EXPONENT:
            if (ch >= '0' && ch <= '9') {
                if (lexer->digit_count == 3) {
                    ERROR("Decimal exponent must be 3 digits or less");
                } else
                    add_safe_digit(lexer, ch - '0', 10);
            } else if (!lexer->digit_count) {
                ERROR("No digits after decimal exponent delimiter");
            } else {
                set_exponent(lexer, 10);
                EMIT_FLOAT();
                lexer->state = SCAN_EXPR;
                BACK_UP;
            }
            break;

        case SCAN_HEX_FLOAT:
            switch (ch) {
            case '.':
                lexer->float_fraction_multiplier = 1;
                lexer->state = SCAN_HEX_FRACTION;
                break;

            case 'p':
            case 'P':
                lexer->state = SCAN_HEX_EXPONENT_SIGN;
                break;

            default:
                digit_value = hex_digit_to_int(ch);
                if (digit_value != -1)
                    add_float_digit(lexer, digit_value, 16);
                else {
                    if (EMIT_FLOAT())
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_HEX_FRACTION:
            switch (ch) {
            case 'p':
            case 'P':
                lexer->state = SCAN_HEX_EXPONENT_SIGN;
                break;

            default:
                digit_value = hex_digit_to_int(ch);
                if (digit_value  != -1)
                    add_float_fraction_digit(lexer, digit_value, 16);
                else {
                    if (EMIT_FLOAT())
                        lexer->state = SCAN_EXPR;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_HEX_EXPONENT_SIGN:
            if (ch == '-')
                lexer->exponent_sign = -1;
            else {
                lexer->exponent_sign = 1;
                BACK_UP;
            }
            lexer->int_value = 0;
            lexer->digit_count = 0;
            lexer->state = SCAN_HEX_EXPONENT;
            break;

        case SCAN_HEX_EXPONENT:
            digit_value = hex_digit_to_int(ch);
            if (digit_value != -1) {
                if (lexer->digit_count == 2) {
                    ERROR("Hex exponent must be 2 digits or less");
                } else
                    add_safe_digit(lexer, digit_value, 16);
            } else if (!lexer->digit_count) {
                ERROR("No digits after hex exponent delimiter");
            } else {
                set_exponent(lexer, 16);
                EMIT_FLOAT();
                lexer->state = SCAN_EXPR;
                BACK_UP;
            }
            break;

        default:
            ERROR("Internal: Unknown lexer state %d", lexer->state);
            break;
        }
    }
}

void gcode_lexer_finish(GCodeLexer* lexer) {
    // A final newline will flush any dangling statement and have no effect
    // otherwise
    if (lexer->state != SCAN_NEWLINE)
        gcode_lexer_scan(lexer, "\n", 1);
}

void gcode_lexer_reset(GCodeLexer* lexer) {
    lexer->state = SCAN_NEWLINE;
    lexer->token_length = 0;
    lexer->line = 1;
    lexer->column = 0;
}

void gcode_lexer_delete(GCodeLexer* lexer) {
    gcode_error_delete(lexer->error);
    free(lexer->token_str);
    free(lexer);
}
