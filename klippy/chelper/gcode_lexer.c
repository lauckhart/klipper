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
    SCAN_COMPLETE,
    SCAN_LINENO,
    SCAN_AFTER_LINENO,
    SCAN_STATEMENT_WHITESPACE,
    SCAN_WORD,
    SCAN_COMMENT,
    SCAN_EMPTY_LINE_COMMENT,
    SCAN_EXPR_WHITESPACE,
    SCAN_AFTER_EXPR,
    SCAN_SYMBOL,
    SCAN_IDENTIFIER,
    SCAN_STRING,
    SCAN_STRING_ESCAPE,
    SCAN_STRING_OCTAL,
    SCAN_STRING_HEX,
    SCAN_STRING_LOW_UNICODE,
    SCAN_STRING_HIGH_UNICODE,
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
    bool (*error)(void*, const char* format, ...);
    bool (*keyword)(void*, gcode_keyword_t id);
    bool (*identifier)(void*, const char* value);
    bool (*str_literal)(void*, const char* value);
    bool (*int_literal)(void*, int64_t value);
    bool (*float_literal)(void*, double value);

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
};

GCodeLexer* gcode_lexer_new(
    void* context,
    bool (*error)(void*, const char* format, ...),
    bool (*keyword)(void*, gcode_keyword_t id),
    bool (*identifier)(void*, const char* value),
    bool (*str_literal)(void*, const char* value),
    bool (*int_literal)(void*, int64_t value),
    bool (*float_literal)(void*, double value)
) {
    GCodeLexer* lexer = malloc(sizeof(GCodeLexer));
    if (!lexer) {
        error(context, "Out of memory (gcode_lexer_new)");
        return NULL;
    }

    lexer->context = context;

    lexer->error = error;
    lexer->keyword = keyword;
    lexer->identifier = identifier;
    lexer->str_literal = str_literal;
    lexer->int_literal = int_literal;
    lexer->float_literal = float_literal;
    lexer->expr_nesting = 0;

    lexer->token_str = NULL;
    lexer->token_length = lexer->token_limit = 0;

    return lexer;
}

static bool gcode_token_alloc(GCodeLexer* lexer) {
    if (lexer->token_str == NULL) {
        lexer->token_limit = 128;
        lexer->token_str = malloc(lexer->token_limit);
        if (!lexer->token_str) {
            lexer->error(lexer->context, "Out of memory (gcode_token_alloc)");
            lexer->state = SCAN_ERROR;
            return false;
        }
    } else {
        lexer->token_limit *= 2;
        lexer->token_str = realloc(lexer->token_str, lexer->token_limit);
        if (!lexer->token_str) {
            lexer->error(lexer->context, "Out of memory (gcode_token_alloc)");
            lexer->state = SCAN_ERROR;
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
    wctomb(buf, lexer->int_value);
    for (char* p = buf; *p; p++)
        if (!token_char(lexer, *p))
            return false;
    return true;
}

static inline bool terminate_token(GCodeLexer* lexer) {
    return token_char(lexer, '\0');
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
    lexer->int_value = lexer->int_value * base + value;
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

#define ERROR(args...) { \
    lexer->error(lexer->context, args); \
    lexer->state = SCAN_ERROR; \
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

    if (ch == '\n')
        lexer->state = SCAN_NEWLINE;

    return true;
}

#define EMIT_CHAR_SYMBOL() emit_char_symbol(lexer, ch)

static inline bool emit_str(GCodeLexer* lexer) {
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
    if (!lexer->int_literal(lexer->context, value)) {
        lexer->state = SCAN_ERROR;
        return false;
    }
    return true;
}

#define EMIT_INT() emit_int(lexer, lexer->int_value)

static inline bool emit_float(GCodeLexer* lexer, int value) {
    if (!lexer->float_literal(lexer->context, value)) {
        lexer->state = SCAN_ERROR;
        return false;
    }
    return true;
}

#define EMIT_FLOAT() emit_float(lexer, lexer->float_value)

static inline bool emit_keyword_or_identifier(GCodeLexer* lexer) {
    GET_KEYWORD_ID;

    bool result;
    if (id == -1)
        result = lexer->identifier(lexer, lexer->token_str);
    else
        result = lexer->keyword(lexer, id);

    free_token(lexer);
    return result;
}

#define EMIT_BRIDGE() emit_char_symbol(lexer, '\xff')

#define DIGIT_EXCEEDS(value, max, base) \
    lexer->int_value > (max - value) / base

bool add_digit(GCodeLexer* lexer, uint8_t value, uint8_t base, int64_t max,
    const char* err)
{
    if (DIGIT_EXCEEDS(value, base, max)) {
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
#define BACK_UP buffer--;
#define CASE_STR_ESC(esc_ch, ch) \
    case esc_ch: \
        if (TOKEN_CHAR(ch)) \
            lexer->state = SCAN_STRING; \
        break;

static const int UNICODE_MAX = 0x10ffff;

// Get ready for monster switch statement.  Two reasons for this:
//   - Performance (no function call overhead)
//   - Incremental scanning (buffer may terminate anywhere in a statement)
void gcode_lexer_scan(GCodeLexer* lexer, const char* buffer, size_t length) {
    const char* end = buffer + length;
    int8_t digit_value;
    for (char ch = *buffer; buffer < end; ch = (*++buffer))
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
                    lexer->state = SCAN_STATEMENT_WHITESPACE;
                    break;
            }
            break;

        case SCAN_ERROR:
            if (ch == '\n')
                EMIT_CHAR_SYMBOL();
            else if (ch == '\0')
                lexer->state = SCAN_COMPLETE;
            break;

        case SCAN_COMPLETE:
            return;

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
                        lexer->state = SCAN_EXPR_WHITESPACE;
                    break;
            }
            break;

        case SCAN_AFTER_LINENO:
            switch (ch) {
                case '\n':
                CASE_SPACE
                    break;

                case ';':
                    lexer->state = SCAN_EMPTY_LINE_COMMENT;
                    break;

                default:
                    BACK_UP;
                    lexer->state = SCAN_STATEMENT_WHITESPACE;
                    break;
            }
            break;
            
        case SCAN_STATEMENT_WHITESPACE:
            switch (ch) {
                case ENTER_EXPR:
                    if (EMIT_CHAR_SYMBOL())
                        lexer->state = SCAN_EXPR_WHITESPACE;
                    break;

                case '\n':
                    EMIT_CHAR_SYMBOL();
                    break;

                case ';':
                    lexer->state = SCAN_COMMENT;
                    break;

                CASE_SPACE
                    break;

                case '\0':
                    lexer->state = SCAN_COMPLETE;
                    return;

                default:
                    lexer->state = SCAN_WORD;
                    BACK_UP;
                    break;
            }
            break;

        case SCAN_WORD:
            switch (ch) {
                case '\n':
                    emit_str(lexer);
                    EMIT_CHAR_SYMBOL();
                    break;

                CASE_SPACE
                    if (emit_str(lexer))
                        lexer->state = SCAN_STATEMENT_WHITESPACE;
                    break;

                case ';':
                    if (emit_str(lexer))
                        lexer->state = SCAN_COMMENT;
                    break;

                case ENTER_EXPR:
                    if (emit_str(lexer) && EMIT_BRIDGE()
                        && EMIT_CHAR_SYMBOL()
                    )
                        lexer->state = SCAN_EXPR_WHITESPACE;
                    break;

                default:
                    TOKEN_CHAR_UPPER();
                    break;
            }
            break;

        case SCAN_COMMENT:
            if (ch == '\n')
                EMIT_CHAR_SYMBOL();
            break;

        case SCAN_EMPTY_LINE_COMMENT:
            if (ch == '\n')
                lexer->state = SCAN_NEWLINE;
            break;

        case SCAN_EXPR_WHITESPACE:
            switch (ch) {
                case '\n':
                    ERROR(lexer->context, "Unterminated expression");
                    lexer->state = SCAN_NEWLINE;
                    break;

                CASE_SPACE
                    break;

                case ENTER_EXPR:
                    if (EMIT_CHAR_SYMBOL())
                        lexer->expr_nesting++;
                    break;

                case EXIT_EXPR:
                    if (EMIT_CHAR_SYMBOL())
                        if (lexer->expr_nesting)
                            lexer->expr_nesting--;
                        else
                            lexer->state = SCAN_AFTER_EXPR;
                    else
                        lexer->expr_nesting = 0;
                    break;

                case '0':
                    lexer->state = SCAN_NUMBER_BASE;
                    break;

                default:
                    if (ch >= '1' && ch <= '9')
                        lexer->state = SCAN_DECIMAL;
                    else if (is_ident_char(ch))
                        lexer->state = SCAN_IDENTIFIER;
                    else
                        lexer->state = SCAN_SYMBOL;
                    BACK_UP;
                    break;
            }
            break;

        case SCAN_AFTER_EXPR:
            switch (ch) {
                case '\0':
                case '\n':
                case ';':
                CASE_SPACE
                    lexer->state = SCAN_STATEMENT_WHITESPACE;
                    break;

                default:
                    EMIT_BRIDGE();
                    break;
            }
            BACK_UP;
            break;

        case SCAN_SYMBOL:
            if (is_ident_char(ch)
                || ch == ' '
                || ch == '\t'
                || ch == '\v'
                || ch == '\r'
                || ch == '\n'
            ) {
                if (!emit_symbol(lexer))
                    break;
                if (ch == '\n') {
                    ERROR("Unterminated expression");
                    lexer->state = SCAN_NEWLINE;
                } else {
                    lexer->state = SCAN_EXPR_WHITESPACE;
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
                lexer->state = SCAN_EXPR_WHITESPACE;
                BACK_UP;
            }
            break;

        case SCAN_STRING:
            switch (ch) {
                case '\\':
                    lexer->state = SCAN_STRING_ESCAPE;
                    break;

                case '"':
                    if (emit_str(lexer))
                        lexer->state = SCAN_EXPR_WHITESPACE;
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

        case SCAN_STRING_ESCAPE:
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
                lexer->state = SCAN_STRING_HEX;
                BACK_UP;
                break;

            case 'u':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_STRING_LOW_UNICODE;
                BACK_UP;
                break;

            case 'U':
                lexer->int_value = 0;
                lexer->digit_count = 0;
                lexer->state = SCAN_STRING_HIGH_UNICODE;
                BACK_UP;
                break;

            case '\n':
                ERROR("Unterminated string");
                free_token(lexer);
                lexer->state = SCAN_NEWLINE;
                break;

            default:
                if (ch >= 0 && ch <= 9) {
                    lexer->int_value = 0;
                    lexer->digit_count = 0;
                    lexer->state = SCAN_STRING_OCTAL;
                    BACK_UP;
                } else {
                    ERROR("Illegal string escape \\%c", ch);
                    free_token(lexer);
                }
                break;
            }
            break;

        case SCAN_STRING_OCTAL:
            if (ch >= '0' && ch <= '7') {
                if (add_digit(lexer, ch - '0', 8, 255,
                              "Octal escape (\\nnn) exceeds byte value")
                    && lexer->digit_count == 3
                    && TOKEN_CHAR(lexer->int_value)
                )
                    lexer->state = SCAN_EXPR_WHITESPACE;
            } else if (ch == '8' || ch == '9') {
                ERROR("Illegal digit in octal escape (\\nnn)");
                free_token(lexer);
            } else {
                if (TOKEN_CHAR(lexer->int_value))
                    lexer->state = SCAN_STRING;
                BACK_UP;
            }
            break;

        case SCAN_STRING_HEX:
            digit_value = hex_digit_to_int(ch);
            if (digit_value == -1) {
                if (!lexer->digit_count) {
                    ERROR("Hex string escape (\\x) requires at least one "
                          "digit");
                    free_token(lexer);
                    break;
                }
                if (TOKEN_CHAR(lexer->int_value))
                    lexer->state = SCAN_STRING;
                BACK_UP;
            }
            add_digit(lexer, digit_value, 16, 255,
                      "Hex escape exceeds byte value");
            break;

        case SCAN_STRING_LOW_UNICODE:
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
                lexer->state = SCAN_STRING;
            break;

        case SCAN_STRING_HIGH_UNICODE:
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
                lexer->state = SCAN_STRING;
            break;

        case SCAN_NUMBER_BASE:
            switch (ch) {
            case 'b':
            case 'B':
                lexer->int_value = 0;
                lexer->state = SCAN_BINARY;
                break;

            case 'x':
            case 'X':
                lexer->int_value = 0;
                lexer->state = SCAN_HEX;
                break;

            default:
                if (ch >= 0 && ch <= 9) {
                    lexer->int_value = 0;
                    lexer->state = SCAN_OCTAL;
                    BACK_UP;
                } else {
                    if (emit_int(lexer, 0))
                        lexer->state = SCAN_EXPR_WHITESPACE;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_DECIMAL:
            switch (ch) {
            case '.':
                lexer->float_value = lexer->int_value;
                lexer->state = SCAN_DECIMAL_FRACTION;
                lexer->digit_count = 0;
                break;

            case 'e':
            case 'E':
                lexer->float_value = lexer->int_value;
                lexer->state = SCAN_DECIMAL_EXPONENT_SIGN;
                break;

            default:
                if (ch >= '0' && ch <= '9') {
                    if (DIGIT_EXCEEDS(ch - '0', INT64_MAX, 10)) {
                        lexer->float_value = lexer->int_value;
                        lexer->state = SCAN_DECIMAL_FLOAT;
                    } else
                        add_safe_digit(lexer, ch - '0', 10);
                } else {
                    if (emit_int(lexer, lexer->int_value))
                        lexer->state = SCAN_EXPR_WHITESPACE;
                    BACK_UP;
                }
                break;
            }
            break;

        case SCAN_HEX:
            switch (ch) {
            case '.':
                lexer->float_value = lexer->int_value;
                lexer->state = SCAN_HEX_FRACTION;
                lexer->digit_count = 0;
                break;

            case 'p':
            case 'P':
                lexer->float_value = lexer->int_value;
                lexer->state = SCAN_HEX_EXPONENT_SIGN;
                break;

            default:
                digit_value = hex_digit_to_int(ch);
                if (digit_value != -1) {
                    if (DIGIT_EXCEEDS(digit_value, INT64_MAX, 16)) {
                        lexer->float_value = lexer->int_value;
                        lexer->state = SCAN_HEX_FLOAT;
                    } else
                        add_safe_digit(lexer, digit_value, 10);
                } else {
                    if (EMIT_INT())
                        lexer->state = SCAN_EXPR_WHITESPACE;
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
                    lexer->state = SCAN_EXPR_WHITESPACE;
                BACK_UP;
            }
            break;

        case SCAN_OCTAL:
            if (ch >= '0' && ch <= '7') {
                add_digit(lexer, ch - '0', 2, INT64_MAX,
                    "Octal literal exceeds maximum value");
            } else if (ch == '.') {
                ERROR("Fractional octal literals not allowed");
            } else if (ch == '8' || ch == '9') {
                ERROR("Illegal octal digit %c", ch);
            } else {
                if (EMIT_INT())
                    lexer->state = SCAN_EXPR_WHITESPACE;
                BACK_UP;
            }
            break;

        case SCAN_DECIMAL_FLOAT:
            switch (ch) {
            case '.':
                lexer->state = SCAN_DECIMAL_FRACTION;
                lexer->digit_count = 0;
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
                        lexer->state = SCAN_EXPR_WHITESPACE;
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
                        lexer->state = SCAN_EXPR_WHITESPACE;
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
                lexer->float_value *= powf(10, lexer->int_value);
                EMIT_FLOAT();
                lexer->state = SCAN_EXPR_WHITESPACE;
                BACK_UP;
            }
            break;

        case SCAN_HEX_FLOAT:
            switch (ch) {
            case '.':
                lexer->state = SCAN_HEX_FRACTION;
                lexer->digit_count = 0;
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
                        lexer->state = SCAN_EXPR_WHITESPACE;
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
                        lexer->state = SCAN_EXPR_WHITESPACE;
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
                lexer->float_value *= powf(16, lexer->int_value);
                EMIT_FLOAT();
                BACK_UP;
            }
            break;

        default:
            ERROR("Internal: Unknown lexer state %d", lexer->state);
            break;
        }
}

void gcode_lexer_finish(GCodeLexer* lexer) {
    if (lexer->state != SCAN_COMPLETE)
        gcode_lexer_scan(lexer, "\0", 1);
    switch (lexer->state) {
    case SCAN_COMPLETE:
    case SCAN_STATEMENT_WHITESPACE:
    case SCAN_COMMENT:
    case SCAN_NEWLINE:
        break;

    case SCAN_STRING:
        lexer->error(lexer->context, "Unterminated string literal");
        break;

    case SCAN_EXPR_WHITESPACE:
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
    lexer->state = SCAN_NEWLINE;
    lexer->token_length = 0;
}

void gcode_lexer_delete(GCodeLexer* lexer) {
    free(lexer->token_str);
    free(lexer);
}
