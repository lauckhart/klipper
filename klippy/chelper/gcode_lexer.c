// G-code lexer implementation
//
// Copyright (C) 2019 Greg Lauckhart <greg@lauckhart.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gcode_lexer.h"
#include <stdlib.h>
#include <math.h>

// These are defined in gcode_parser.keywords.c
struct GCodeKeywordDetail {
    char *name;
    int id;
};
typedef struct GCodeKeywordDetail GCodeKeywordDetail;
GCodeKeywordDetail* gcode_keyword_lookup(register const char *str,
                                         register size_t len);

typedef enum state_t {
    PARSING_WHITESPACE,
    PARSING_COMPLETE,
    PARSING_WORD,
    PARSING_COMMENT,
    PARSING_EXPRESSION,
    PARSING_SYMBOL,
    PARSING_IDENTIFIER,
    PARSING_STRING,
    PARSING_STRING_ESCAPE,
    PARSING_STRING_OCTAL,
    PARSING_STRING_HEX,
    PARSING_STRING_LOW_UNICODE,
    PARSING_STRING_HIGH_UNICODE,
    PARSING_NUMBER_BASE,
    PARSING_DECIMAL,
    PARSING_HEX,
    PARSING_BINARY,
    PARSING_OCTAL,
    PARSING_DECIMAL_FLOAT,
    PARSING_DECIMAL_FRACTION,
    PARSING_DECIMAL_EXPONENT_SIGN,
    PARSING_DECIMAL_EXPONENT,
    PARSING_HEX_FLOAT,
    PARSING_HEX_FRACTION,
    PARSING_HEX_EXPONENT_SIGN,
    PARSING_HEX_EXPONENT
} state_t;

struct GCodeLexer {
    bool (*error)(void*, const char* format, ...);
    bool (*keyword)(void*, gcode_keyword_t id);
    bool (*identifier)(void*, const char* value);
    bool (*string_literal)(void*, const char* value);
    bool (*int_literal)(void*, int64_t value);
    bool (*float_literal)(void*, double value);

    void*   context;
    state_t state;
    char*   token_str;
    size_t  token_length;
    size_t  token_limit;
    size_t  expr_nesting;
    int64_t integer_value;
    double  float_value;
    int8_t  exponent_sign;
    int8_t  digit_count;
};

GCodeLexer* gcode_lexer_new(
    void* context,
    bool (*error)(void*, const char* format, ...),
    bool (*keyword)(void*, gcode_keyword_t id),
    bool (*identifier)(void*, const char* value),
    bool (*string_literal)(void*, const char* value),
    bool (*int_literal)(void*, int64_t value),
    bool (*float_literal)(void*, double value)
) {
    GCodeLexer* lexer = malloc(sizeof(GCodeLexer));
    if (!lexer) {
        error(context, "Out of memory");
        return NULL;
    }

    lexer->context = context;

    lexer->error = error;
    lexer->keyword = keyword;
    lexer->identifier = identifier;
    lexer->string_literal = string_literal;
    lexer->int_literal = int_literal;
    lexer->float_literal = float_literal;
    lexer->expr_nesting = 0;

    lexer->token_str = NULL;
    lexer->token_length = lexer->token_limit = 0;

    return lexer;
}

static bool gcode_token_alloc(GCodeLexer* lexer) {
    if (lexer->token_str == NULL) {
        lexer->token_str = malloc(128);
        if (!lexer->token_str) {
            lexer->error(lexer->context, "Out of memory");
            return false;
        }
    } else {
        lexer->token_length *= 2;
        lexer->token_str = realloc(lexer->token_str, lexer->token_length);
        if (!lexer->token_str) {
            lexer->error(lexer->context, "Out of memory");
            return false;
        }
    }
    return true;
}

static inline bool token_char(GCodeLexer* lexer, const char ch) {
    if (lexer->token_length == lexer->token_limit
        && !gcode_token_alloc(lexer))
    {
        lexer->state = PARSING_COMPLETE;
        return false;
    }
    lexer->token_str[lexer->token_length++] = ch;
    return true;
}

static bool gcode_add_str_wchar(GCodeLexer* lexer) {
    char buf[MB_CUR_MAX];
    wctomb(buf, lexer->integer_value);
    for (char* p = buf; *p; p++)
        if (!token_char(lexer, *p)) {
            lexer->state = PARSING_COMPLETE;
            return false;
        }
    return true;
}

static inline bool complete_token(GCodeLexer* lexer) {
    if (!token_char(lexer, '\0')) {
        lexer->state = PARSING_COMPLETE;
        return false;
    }
}

static inline char hex_digit_to_int(char ch) {
    if (ch >= 0 && ch <= 9)
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a';
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    return -1;
}

static inline void add_safe_digit(GCodeLexer* lexer, int8_t value,
                           int8_t base)
 {
    lexer->integer_value = lexer->integer_value * base + value;
    lexer->digit_count++;
}

static inline void add_float_digit(GCodeLexer* lexer, float value,
                            int base)
{
    lexer->float_value = lexer->float_value * base + value;
}

static inline void add_float_fraction_digit(GCodeLexer* lexer, float value,
                                     int base)
{
    lexer->float_value += value
        / powf(base, lexer->exponent_sign * lexer->digit_count);
    lexer->digit_count++;
}

static inline bool is_ident_char(char ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || ch == '_'
        || ch == '$';
}

#define EMIT(callback, args...) { \
    if (!lexer->callback(lexer->context, args)) { \
        lexer->state = PARSING_COMPLETE; \
        return false; \
    } \
}

#define ERROR(args...) { \
    lexer->error(lexer->context, args); \
    lexer->state = PARSING_COMPLETE; \
    return false; \
}

#define COMPLETE_TOKEN() { \
    if (!complete_token(lexer)) { \
        return false; \
    } \
}

static inline bool emit_symbol(GCodeLexer* lexer) {
    GCodeKeywordDetail* detail = gcode_keyword_lookup(lexer->token_str,
                                                    lexer->token_length);
    if (detail) {
        EMIT(keyword, detail->id);
        return true;
    }

    COMPLETE_TOKEN();
    ERROR("Illegal operator '%s'", lexer->token_str);

    return false;
}

#define EMIT_IDENTIFIER() { \
    COMPLETE_TOKEN(); \
    EMIT(identifier, lexer->token_str); \
    lexer->token_length = 0; \
}

static inline bool emit_keyword_or_identifier(GCodeLexer* lexer) {
    GCodeKeywordDetail* detail = gcode_keyword_lookup(lexer->token_str,
                                                    lexer->token_length);
    if (detail) {
        EMIT(keyword, detail->id);
    } else {
        EMIT_IDENTIFIER();
    }

    return true;
}

#define EMIT_SYMBOL() { \
    if (!emit_symbol(lexer)) \
        return false; \
}

#define DIGIT_EXCEEDS(value, max, base) \
    lexer->integer_value > (max - value) / base

#define ADD_DIGIT(value, base, max, error) { \
    if (DIGIT_EXCEEDS(value, base, max)) \
        ERROR(error); \
    add_safe_digit(lexer, value, base); \
}

#define TOKEN_CHAR(ch) { \
    if (!token_char(lexer, ch)) { \
        lexer->state = PARSING_COMPLETE; \
        return false; \
    } \
}

#define TOKEN_CHAR_UPPER(ch) TOKEN_CHAR(ch >= 'a' && ch <= 'z' ? ch - 32 : ch)

#define ADD_STR_WCHAR() { \
    if (!gcode_add_str_wchar(lexer)) \
        return false; \
}

#define CASE_SPACE case ' ': case '\t': case '\v': case '\r'
#define BACK_UP buffer--;
#define CASE_STR_ESC(esc_ch, ch) case esc_ch: TOKEN_CHAR(ch); break;

static const int UNICODE_MAX = 0x10ffff;

// Get ready for monster switch statement.  Two reasons for this:
//   - Performance (no function call overhead)
//   - Incremental scanning (buffer may terminate anywhere in a statement)
bool gcode_lexer_scan(GCodeLexer* lexer, const char* buffer,
                      size_t length)
{
    const char* end = buffer + length;
    int8_t digit_value;
    for (char ch = *buffer; buffer < end; buffer++)
        switch (lexer->state) {
            case PARSING_WHITESPACE:
                switch (ch) {
                    case '(':
                        TOKEN_CHAR('(');
                        EMIT_SYMBOL();
                        lexer->state = PARSING_EXPRESSION;
                        break;

                    case '\n':
                        TOKEN_CHAR('\n');
                        EMIT_SYMBOL();
                        break;

                    case ';':
                        lexer->state = PARSING_COMMENT;
                        break;

                    CASE_SPACE:
                        break;

                    case '\0':
                        lexer->state = PARSING_COMPLETE;
                        return true;

                    default:
                        lexer->state = PARSING_WORD;
                        break;
                }
                break;

            case PARSING_COMPLETE:
                return false;

            case PARSING_WORD:
                switch (ch) {
                    case '\n':
                        EMIT_IDENTIFIER();
                        if (ch == '\n')
                            ERROR("Unterminated expression");
                        break;

                    CASE_SPACE:
                        EMIT_IDENTIFIER();
                        lexer->state = PARSING_WHITESPACE;
                        break;

                    case ';':
                        EMIT_IDENTIFIER();
                        lexer->state = PARSING_COMMENT;
                        break;

                    case '(':
                        EMIT_IDENTIFIER();
                        lexer->state = PARSING_EXPRESSION;
                        break;

                    default:
                        TOKEN_CHAR_UPPER(ch);
                        break;
                }
                return false;

            case PARSING_COMMENT:
                if (ch == '\n') {
                    TOKEN_CHAR('\n');
                    EMIT_SYMBOL();
                    lexer->state = PARSING_WHITESPACE;
                }
                break;

            case PARSING_EXPRESSION:
                switch (ch) {
                    case '\n':
                        ERROR("Unterminated expression");
                        break;

                    CASE_SPACE:
                        break;

                    case '(':
                        TOKEN_CHAR('(');
                        EMIT_SYMBOL();
                        lexer->expr_nesting++;
                        break;

                    case ')':
                        TOKEN_CHAR(')');
                        EMIT_SYMBOL();
                        if (!lexer->expr_nesting)
                            lexer->state = PARSING_WHITESPACE;
                        else
                            lexer->expr_nesting--;
                        break;

                    case '0':
                        lexer->state = PARSING_NUMBER_BASE;
                        break;

                    default:
                        if (ch >= '1' && ch <= '9')
                            lexer->state = PARSING_DECIMAL;
                        else if (is_ident_char(ch))
                            lexer->state = PARSING_IDENTIFIER;
                        else
                            lexer->state = PARSING_SYMBOL;
                        BACK_UP;
                        break;
                }
                break;

            case PARSING_SYMBOL:
                if (is_ident_char(ch)
                    || ch == ' '
                    || ch == '\t'
                    || ch == '\v'
                    || ch == '\r'
                    || ch == '\n'
                ) {
                    if (!emit_symbol(lexer))
                        return false;
                    if (ch == '\n')
                        ERROR("Unterminated expression");
                    lexer->state = PARSING_EXPRESSION;
                    BACK_UP;
                }
                break;

            case PARSING_IDENTIFIER:
                if (is_ident_char(ch)) {
                    TOKEN_CHAR_UPPER(ch);
                } else {
                    if (!emit_keyword_or_identifier(lexer))
                        return false;
                    lexer->state = PARSING_EXPRESSION;
                    BACK_UP;
                }
                break;

            case PARSING_STRING:
                switch (ch) {
                    case '\\':
                        lexer->state = PARSING_STRING_ESCAPE;
                        break;

                    case '"':
                        COMPLETE_TOKEN();
                        EMIT(string_literal, lexer->token_str);
                        lexer->state = PARSING_EXPRESSION;
                        break;

                    case '\n':
                        ERROR("Unterminated string");
                        break;

                    default:
                        TOKEN_CHAR(ch);
                        break;
                }
                break;

            case PARSING_STRING_ESCAPE:
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
                        lexer->integer_value = 0;
                        lexer->digit_count = 0;
                        lexer->state = PARSING_STRING_HEX;
                        BACK_UP;
                        break;

                    case 'u':
                        lexer->integer_value = 0;
                        lexer->digit_count = 0;
                        lexer->state = PARSING_STRING_LOW_UNICODE;
                        BACK_UP;
                        break;

                    case 'U':
                        lexer->integer_value = 0;
                        lexer->digit_count = 0;
                        lexer->state = PARSING_STRING_HIGH_UNICODE;
                        BACK_UP;
                        break;

                    case '\n':
                        ERROR("Unterminated string");
                        break;

                    default:
                        if (ch >= 0 && ch <= 9) {
                            lexer->integer_value = 0;
                            lexer->digit_count = 0;
                            lexer->state = PARSING_STRING_OCTAL;
                            BACK_UP;
                        } else
                            ERROR("Illegal string escape \\%c", ch);
                        break;
                }
                break;

            case PARSING_STRING_OCTAL:
                if (ch >= 0 && ch <= 7) {
                    ADD_DIGIT(ch - '0', 8, 255,
                        "Octal escape (\\nnn) exceeds byte value");
                    if (lexer->digit_count == 3) {
                        TOKEN_CHAR(lexer->integer_value);
                        lexer->state = PARSING_STRING;
                    }
                } else if (ch == 8 || ch == 9) {
                    ERROR("Illegal digit in octal escape (\\nnn)");
                } else {
                    TOKEN_CHAR(lexer->integer_value);
                    BACK_UP;
                    lexer->state = PARSING_STRING;
                }
                break;

            case PARSING_STRING_HEX:
                digit_value = hex_digit_to_int(ch);
                if (digit_value == -1) {
                    if (!lexer->digit_count)
                        ERROR("Hex string escape (\\x) requires at least one "
                              "digit");
                    TOKEN_CHAR(lexer->integer_value);
                    BACK_UP;
                    lexer->state = PARSING_STRING;
                }
                ADD_DIGIT(digit_value, 16, 255,
                          "Hex escape exceeds byte value");
                break;

            case PARSING_STRING_LOW_UNICODE:
                digit_value = hex_digit_to_int(ch);
                if (digit_value == -1)
                    ERROR("Low unicode escape (\\u) requires exactly four "
                          "digits");
                add_safe_digit(lexer, digit_value, 16);
                if (lexer->digit_count == 4) {
                    ADD_STR_WCHAR();
                    lexer->state = PARSING_STRING;
                }
                break;

            case PARSING_STRING_HIGH_UNICODE:
                digit_value = hex_digit_to_int(ch);
                if (digit_value == -1)
                    ERROR("High unicode escape (\\U) requires exactly eight "
                          "digits")
                ADD_DIGIT(digit_value, 16, UNICODE_MAX,
                          "High unicode escape (\\U) exceeds unicode value");
                if (lexer->digit_count == 8) {
                    ADD_STR_WCHAR();
                    lexer->state = PARSING_STRING;
                }
                break;

            case PARSING_NUMBER_BASE:
                switch (ch) {
                    case 'b':
                    case 'B':
                        lexer->integer_value = 0;
                        lexer->state = PARSING_BINARY;
                        break;

                    case 'x':
                    case 'X':
                        lexer->integer_value = 0;
                        lexer->state = PARSING_HEX;
                        break;

                    default:
                        if (ch >= 0 && ch <= 9) {
                            lexer->integer_value = 0;
                            lexer->state = PARSING_OCTAL;
                            BACK_UP;
                        } else {
                            EMIT(int_literal, 0);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_DECIMAL:
                switch (ch) {
                    case '.':
                        lexer->float_value = lexer->integer_value;
                        lexer->state = PARSING_DECIMAL_FRACTION;
                        lexer->digit_count = 0;
                        break;

                    case 'e':
                    case 'E':
                        lexer->float_value = lexer->integer_value;
                        lexer->state = PARSING_DECIMAL_EXPONENT_SIGN;
                        break;

                    default:
                        if (ch >= '0' && ch <= '9') {
                            if (DIGIT_EXCEEDS(ch - '0', INT64_MAX, 10)) {
                                lexer->float_value = lexer->integer_value;
                                lexer->state = PARSING_DECIMAL_FLOAT;
                            } else
                                add_safe_digit(lexer, ch - '0', 10);
                        } else {
                            EMIT(int_literal, lexer->integer_value);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_HEX:
                switch (ch) {
                    case '.':
                        lexer->float_value = lexer->integer_value;
                        lexer->state = PARSING_HEX_FRACTION;
                        lexer->digit_count = 0;
                        break;

                    case 'p':
                    case 'P':
                        lexer->float_value = lexer->integer_value;
                        lexer->state = PARSING_HEX_EXPONENT_SIGN;
                        break;

                    default:
                        digit_value = hex_digit_to_int(ch);
                        if (digit_value != -1) {
                            if (DIGIT_EXCEEDS(digit_value, INT64_MAX, 16)) {
                                lexer->float_value = lexer->integer_value;
                                lexer->state = PARSING_HEX_FLOAT;
                            } else
                                add_safe_digit(lexer, digit_value, 10);
                        } else {
                            EMIT(int_literal, lexer->integer_value);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_BINARY:
                if (ch == '0' || ch == '1') {
                    ADD_DIGIT(ch - '0', 2, INT64_MAX,
                        "Binary literal exceeds maximum value");
                } else if (ch == '.') {
                    ERROR("Fractional binary literals not allowed");
                } else if (ch >= '2' && ch <= '9') {
                    ERROR("Illegal binary digit %c", ch);
                } else {
                    EMIT(int_literal, lexer->integer_value);
                    lexer->state = PARSING_EXPRESSION;
                    BACK_UP;
                }
                break;

            case PARSING_OCTAL:
                if (ch >= '0' && ch <= '7') {
                    ADD_DIGIT(ch - '0', 2, INT64_MAX,
                        "Octal literal exceeds maximum value");
                } else if (ch == '.') {
                    ERROR("Fractional octal literals not allowed");
                } else if (ch == '8' || ch == '9') {
                    ERROR("Illegal octal digit %c", ch);
                } else {
                    EMIT(int_literal, lexer->integer_value);
                    lexer->state = PARSING_EXPRESSION;
                    BACK_UP;
                }
                break;

            case PARSING_DECIMAL_FLOAT:
                switch (ch) {
                    case '.':
                        lexer->state = PARSING_DECIMAL_FRACTION;
                        lexer->digit_count = 0;
                        break;

                    case 'e':
                    case 'E':
                        lexer->state = PARSING_DECIMAL_EXPONENT_SIGN;
                        break;

                    default:
                        if (ch >= '0' && ch <= '9')
                            add_float_digit(lexer, ch - '0', 10);
                        else {
                            EMIT(float_literal, lexer->float_value);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_DECIMAL_FRACTION:
                switch (ch) {
                    case 'e':
                    case 'E':
                        lexer->state = PARSING_DECIMAL_EXPONENT_SIGN;
                        break;

                    default:
                        if (ch >= '0' && ch <= '9')
                            add_float_fraction_digit(lexer, ch - '0', 10);
                        else {
                            EMIT(float_literal, lexer->float_value);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_DECIMAL_EXPONENT_SIGN:
                if (ch == '-')
                    lexer->exponent_sign = -1;
                else {
                    lexer->exponent_sign = 1;
                    BACK_UP;
                }
                lexer->integer_value = 0;
                lexer->digit_count = 0;
                lexer->state = PARSING_DECIMAL_EXPONENT;
                break;

            case PARSING_DECIMAL_EXPONENT:
                if (ch >= '0' && ch <= '9') {
                    if (lexer->digit_count == 3) {
                        ERROR("Decimal exponent must be 3 digits or less");
                    } else
                        add_safe_digit(lexer, ch - '0', 10);
                } else if (!lexer->digit_count) {
                    ERROR("No digits after decimal exponent delimiter");
                } else {
                    lexer->float_value *= powf(10, lexer->integer_value);
                    EMIT(float_literal, lexer->float_value);
                    BACK_UP;
                }
                break;

            case PARSING_HEX_FLOAT:
                switch (ch) {
                    case '.':
                        lexer->state = PARSING_HEX_FRACTION;
                        lexer->digit_count = 0;
                        break;

                    case 'p':
                    case 'P':
                        lexer->state = PARSING_HEX_EXPONENT_SIGN;
                        break;

                    default:
                        digit_value = hex_digit_to_int(ch);
                        if (digit_value != -1)
                            add_float_digit(lexer, digit_value, 16);
                        else {
                            EMIT(float_literal, lexer->float_value);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_HEX_FRACTION:
                switch (ch) {
                    case 'p':
                    case 'P':
                        lexer->state = PARSING_HEX_EXPONENT_SIGN;
                        break;

                    default:
                        digit_value = hex_digit_to_int(ch);
                        if (digit_value  != -1)
                            add_float_fraction_digit(lexer, digit_value, 16);
                        else {
                            EMIT(float_literal, lexer->float_value);
                            lexer->state = PARSING_EXPRESSION;
                            BACK_UP;
                        }
                        break;
                }
                break;

            case PARSING_HEX_EXPONENT_SIGN:
                if (ch == '-')
                    lexer->exponent_sign = -1;
                else {
                    lexer->exponent_sign = 1;
                    BACK_UP;
                }
                lexer->integer_value = 0;
                lexer->digit_count = 0;
                lexer->state = PARSING_HEX_EXPONENT;
                break;

            case PARSING_HEX_EXPONENT:
                digit_value = hex_digit_to_int(ch);
                if (digit_value != -1) {
                    if (lexer->digit_count == 2) {
                        ERROR("Hex exponent must be 2 digits or less");
                    } else
                        add_safe_digit(lexer, digit_value, 16);
                } else if (!lexer->digit_count) {
                    ERROR("No digits after hex exponent delimiter");
                } else {
                    lexer->float_value *= powf(16, lexer->integer_value);
                    EMIT(float_literal, lexer->float_value);
                    BACK_UP;
                }
                break;

            default:
                ERROR("Internal: Unknown lexer state %d", lexer->state);
                break;
        }
}

void gcode_lexer_finish(GCodeLexer* lexer) {
    if (lexer->state != PARSING_COMPLETE)
        gcode_lexer_scan(lexer, "\0", 1);
    switch (lexer->state) {
        case PARSING_COMPLETE:
        case PARSING_WHITESPACE:
        case PARSING_COMMENT:
            break;

        case PARSING_STRING:
            lexer->error(lexer->context, "Unterminated string literal");
            break;

        case PARSING_EXPRESSION:
            lexer->error(lexer->context, "Unterminated expression");
            break;

        default:
            // Parsing \0 should terminate all other states
            lexer->error(lexer->context,
                "Internal error: Lexing terminated in unknown state %d",
                lexer->state);
    }
}

void gcode_lexer_reset(GCodeLexer* lexer) {
    lexer->state = PARSING_WHITESPACE;
}

void gcode_lexer_delete(GCodeLexer* lexer) {
    free(lexer->token_str);
    free(lexer);
}
