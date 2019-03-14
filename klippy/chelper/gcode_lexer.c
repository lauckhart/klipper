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
GCodeKeywordDetail* gcode_keyword_lookup(register const char *str, register size_t len);

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

struct GCodeLexerSession {
    bool (*error)(void*, const char* format, ...);
    bool (*keyword)(void*, gcode_keyword_t id);
    bool (*identifier)(void*, const char* value);
    bool (*string_literal)(void*, const char* value);
    bool (*integer_literal)(void*, int64_t value);
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

GCodeLexerSession* gcode_lexer_start(
    void* context,
    bool (*error)(void*, const char* format, ...),
    bool (*keyword)(void*, gcode_keyword_t id),
    bool (*identifier)(void*, const char* value),
    bool (*string_literal)(void*, const char* value),
    bool (*integer_literal)(void*, int64_t value),
    bool (*float_literal)(void*, double value)
) {
    GCodeLexerSession* session = malloc(sizeof(GCodeLexerSession));
    if (!session) {
        session->error(context, "Out of memory");
        return NULL;
    }

    session->context = context;

    session->error = error;
    session->keyword = keyword;
    session->identifier = identifier;
    session->string_literal = string_literal;
    session->integer_literal = integer_literal;
    session->float_literal = float_literal;
    session->expr_nesting = 0;

    session->token_str = NULL;
    session->token_length = session->token_limit = 0;
}

static bool gcode_token_alloc(GCodeLexerSession* session) {
    if (session->token_str == NULL) {
        session->token_str = malloc(128);
        if (!session->token_str) {
            session->error(session->context, "Out of memory");
            return false;
        }
    } else {
        session->token_length *= 2;
        session->token_str = realloc(session->token_str, session->token_length);
        if (!session->token_str) {
            session->error(session->context, "Out of memory");
            return false;
        }
    }
    return true;
}

static inline bool token_char(GCodeLexerSession* session, const char ch) {
    if (session->token_length == session->token_limit
        && !gcode_token_alloc(session))
    {
        session->state = PARSING_COMPLETE;
        return false;
    }
    session->token_str[session->token_length++] = ch;
    return true;
}

static bool gcode_add_str_wchar(GCodeLexerSession* session) {
    char buf[MB_CUR_MAX];
    wctomb(buf, session->integer_value);
    for (char* p = buf; *p; p++)
        if (!token_char(session, *p)) {
            session->state = PARSING_COMPLETE;
            return false;
        }
    return true;
}

static inline bool complete_token(GCodeLexerSession* session) {
    if (!token_char(session, '\0')) {
        session->state = PARSING_COMPLETE;
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

static inline void add_safe_digit(GCodeLexerSession* session, int8_t value,
                           int8_t base)
 {
    session->integer_value = session->integer_value * base + value;
    session->digit_count++;
}

static inline void add_float_digit(GCodeLexerSession* session, float value,
                            int base)
{
    session->float_value = session->float_value * base + value;
}

static inline void add_float_fraction_digit(GCodeLexerSession* session, float value,
                                     int base)
{
    session->float_value += value
        / powf(base, session->exponent_sign * session->digit_count);
    session->digit_count++;
}

static inline bool is_ident_char(char ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || ch == '_'
        || ch == '$';
}

#define EMIT(callback, args...) { \
    if (!session->callback(session->context, args)) { \
        session->state = PARSING_COMPLETE; \
        return false; \
    } \
}

#define ERROR(args...) { \
    session->error(session->context, args); \
    session->state = PARSING_COMPLETE; \
    return false; \
}

#define COMPLETE_TOKEN() { \
    if (!complete_token(session)) { \
        return false; \
    } \
}

static inline bool emit_symbol(GCodeLexerSession* session) {
    GCodeKeywordDetail* detail = gcode_keyword_lookup(session->token_str,
                                                    session->token_length);
    if (detail) {
        EMIT(keyword, detail->id);
        return true;
    }

    COMPLETE_TOKEN();
    ERROR("Illegal operator '%s'", session->token_str);

    return false;
}

#define EMIT_IDENTIFIER() { \
    COMPLETE_TOKEN(); \
    EMIT(identifier, session->token_str); \
    session->token_length = 0; \
}

static inline bool emit_keyword_or_identifier(GCodeLexerSession* session) {
    GCodeKeywordDetail* detail = gcode_keyword_lookup(session->token_str,
                                                    session->token_length);
    if (detail) {
        EMIT(keyword, detail->id);
    } else {
        EMIT_IDENTIFIER();
    }

    return true;
}

#define EMIT_SYMBOL() { \
    if (!emit_symbol(session)) \
        return false; \
}

#define DIGIT_EXCEEDS(value, max, base) \
    session->integer_value > (max - value) / base

#define ADD_DIGIT(value, base, max, error) { \
    if (DIGIT_EXCEEDS(value, base, max)) \
        ERROR(error); \
    add_safe_digit(session, value, base); \
}

#define TOKEN_CHAR(ch) { \
    if (!token_char(session, ch)) { \
        session->state = PARSING_COMPLETE; \
        return false; \
    } \
}

#define TOKEN_CHAR_UPPER(ch) TOKEN_CHAR(ch >= 'a' && ch <= 'z' ? ch - 32 : ch)

#define ADD_STR_WCHAR() { \
    if (!gcode_add_str_wchar(session)) \
        return false; \
}

#define CASE_WHITESPACE case ' ': case '\t': case '\v': case '\r': case '\n'
#define BACK_UP buffer--;
#define CASE_STR_ESC(esc_ch, ch) case esc_ch: TOKEN_CHAR(ch); break;

static const int UNICODE_MAX = 0x10ffff;

bool gcode_lexer_lex(GCodeLexerSession* session, const char* buffer,
                     size_t length)
{
    const char* end = buffer + length;
    int8_t digit_value;
    for (char ch = *buffer; buffer < end; buffer++)
        switch (session->state) {
            case PARSING_WHITESPACE:
                switch (ch) {
                    case '(':
                        TOKEN_CHAR('(');
                        EMIT_SYMBOL();
                        session->state = PARSING_EXPRESSION;
                        break;

                    case ';':
                        session->state = PARSING_COMMENT;
                        break;

                    CASE_WHITESPACE:
                        break;

                    case '\0':
                        session->state = PARSING_COMPLETE;
                        return true;

                    default:
                        session->state = PARSING_WORD;
                        break;
                }
                break;

            case PARSING_COMPLETE:
                return false;

            case PARSING_WORD:
                switch (ch) {
                    CASE_WHITESPACE:
                        EMIT_IDENTIFIER();
                        session->state = PARSING_WHITESPACE;
                        break;

                    case ';':
                        EMIT_IDENTIFIER();
                        session->state = PARSING_COMMENT;
                        break;

                    case '(':
                        EMIT_IDENTIFIER();
                        session->state = PARSING_EXPRESSION;
                        break;

                    default:
                        TOKEN_CHAR_UPPER(ch);
                        break;
                }
                return false;

            case PARSING_COMMENT:
                if (ch == '\n')
                    session->state = PARSING_WHITESPACE;
                break;

            case PARSING_EXPRESSION:
                switch (ch) {
                    CASE_WHITESPACE:
                        break;

                    case '(':
                        TOKEN_CHAR('(');
                        EMIT_SYMBOL();
                        session->expr_nesting++;
                        break;

                    case ')':
                        TOKEN_CHAR(')');
                        EMIT_SYMBOL();
                        if (!session->expr_nesting)
                            session->state = PARSING_WHITESPACE;
                        else
                            session->expr_nesting--;
                        break;

                    case '0':
                        session->state = PARSING_NUMBER_BASE;
                        break;

                    default:
                        if (ch >= '1' && ch <= '9')
                            session->state = PARSING_DECIMAL;
                        else if (is_ident_char(ch))
                            session->state = PARSING_IDENTIFIER;
                        else
                            session->state = PARSING_SYMBOL;
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
                    if (!emit_symbol(session))
                        return false;
                    session->state = PARSING_EXPRESSION;
                    BACK_UP;
                }
                break;

            case PARSING_IDENTIFIER:
                if (is_ident_char(ch)) {
                    TOKEN_CHAR_UPPER(ch);
                } else {
                    if (!emit_keyword_or_identifier(session))
                        return false;
                    session->state = PARSING_EXPRESSION;
                    BACK_UP;
                }
                break;

            case PARSING_STRING:
                switch (ch) {
                    case '\\':
                        session->state = PARSING_STRING_ESCAPE;
                        break;

                    case '"':
                        COMPLETE_TOKEN();
                        EMIT(string_literal, session->token_str);
                        session->state = PARSING_EXPRESSION;
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
                        session->integer_value = 0;
                        session->digit_count = 0;
                        session->state = PARSING_STRING_HEX;
                        BACK_UP;
                        break;

                    case 'u':
                        session->integer_value = 0;
                        session->digit_count = 0;
                        session->state = PARSING_STRING_LOW_UNICODE;
                        BACK_UP;
                        break;

                    case 'U':
                        session->integer_value = 0;
                        session->digit_count = 0;
                        session->state = PARSING_STRING_HIGH_UNICODE;
                        BACK_UP;
                        break;

                    default:
                        if (ch >= 0 && ch <= 9) {
                            session->integer_value = 0;
                            session->digit_count = 0;
                            session->state = PARSING_STRING_OCTAL;
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
                    if (session->digit_count == 3) {
                        TOKEN_CHAR(session->integer_value);
                        session->state = PARSING_STRING;
                    }
                } else if (ch == 8 || ch == 9) {
                    ERROR("Illegal digit in octal escape (\\nnn)");
                } else {
                    TOKEN_CHAR(session->integer_value);
                    BACK_UP;
                    session->state = PARSING_STRING;
                }
                break;

            case PARSING_STRING_HEX:
                digit_value = hex_digit_to_int(ch);
                if (digit_value == -1) {
                    if (!session->digit_count)
                        ERROR("Hex string escape (\\x) requires at least one "
                              "digit");
                    TOKEN_CHAR(session->integer_value);
                    BACK_UP;
                    session->state = PARSING_STRING;
                }
                ADD_DIGIT(digit_value, 16, 255,
                          "Hex escape exceeds byte value");
                break;

            case PARSING_STRING_LOW_UNICODE:
                digit_value = hex_digit_to_int(ch);
                if (digit_value == -1)
                    ERROR("Low unicode escape (\\u) requires exactly four "
                          "digits");
                add_safe_digit(session, digit_value, 16);
                if (session->digit_count == 4) {
                    ADD_STR_WCHAR();
                    session->state = PARSING_STRING;
                }
                break;

            case PARSING_STRING_HIGH_UNICODE:
                digit_value = hex_digit_to_int(ch);
                if (digit_value == -1)
                    ERROR("High unicode escape (\\U) requires exactly eight "
                          "digits")
                ADD_DIGIT(digit_value, 16, UNICODE_MAX,
                          "High unicode escape (\\U) exceeds unicode value");
                if (session->digit_count == 8) {
                    ADD_STR_WCHAR();
                    session->state = PARSING_STRING;
                }
                break;

            case PARSING_NUMBER_BASE:
                switch (ch) {
                    case 'b':
                    case 'B':
                        session->integer_value = 0;
                        session->state = PARSING_BINARY;
                        break;

                    case 'x':
                    case 'X':
                        session->integer_value = 0;
                        session->state = PARSING_HEX;
                        break;

                    default:
                        if (ch >= 0 && ch <= 9) {
                            session->integer_value = 0;
                            session->state = PARSING_OCTAL;
                            BACK_UP;
                        } else {
                            EMIT(integer_literal, 0);
                            session->state = PARSING_EXPRESSION;
                        }
                        break;
                }
                break;

            case PARSING_DECIMAL:
                switch (ch) {
                    case '.':
                        session->float_value = session->integer_value;
                        session->state = PARSING_DECIMAL_FRACTION;
                        session->digit_count = 0;
                        break;

                    case 'e':
                    case 'E':
                        session->float_value = session->integer_value;
                        session->state = PARSING_DECIMAL_EXPONENT_SIGN;
                        break;

                    default:
                        if (ch >= '0' && ch <= '9') {
                            if (DIGIT_EXCEEDS(ch - '0', INT64_MAX, 10)) {
                                session->float_value = session->integer_value;
                                session->state = PARSING_DECIMAL_FLOAT;
                            } else
                                add_safe_digit(session, ch - '0', 10);
                        } else {
                            EMIT(integer_literal, session->integer_value);
                            BACK_UP;
                            session->state = PARSING_EXPRESSION;
                        }
                        break;
                }
                break;

            case PARSING_HEX:
                switch (ch) {
                    case '.':
                        session->float_value = session->integer_value;
                        session->state = PARSING_HEX_FRACTION;
                        session->digit_count = 0;
                        break;

                    case 'p':
                    case 'P':
                        session->float_value = session->integer_value;
                        session->state = PARSING_HEX_EXPONENT_SIGN;
                        break;

                    default:
                        digit_value = hex_digit_to_int(ch);
                        if (digit_value != -1) {
                            if (DIGIT_EXCEEDS(digit_value, INT64_MAX, 16)) {
                                session->float_value = session->integer_value;
                                session->state = PARSING_HEX_FLOAT;
                            } else
                                add_safe_digit(session, digit_value, 10);
                        } else {
                            EMIT(integer_literal, session->integer_value);
                            BACK_UP;
                            session->state = PARSING_EXPRESSION;
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
                    EMIT(integer_literal, session->integer_value);
                    BACK_UP;
                    session->state = PARSING_EXPRESSION;
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
                    EMIT(integer_literal, session->integer_value);
                    BACK_UP;
                    session->state = PARSING_EXPRESSION;
                }
                break;

            case PARSING_DECIMAL_FLOAT:
                switch (ch) {
                    case '.':
                        session->state = PARSING_DECIMAL_FRACTION;
                        session->digit_count = 0;
                        break;

                    case 'e':
                    case 'E':
                        session->state = PARSING_DECIMAL_EXPONENT_SIGN;
                        break;

                    default:
                        if (ch >= '0' && ch <= '9')
                            add_float_digit(session, ch - '0', 10);
                        else {
                            EMIT(float_literal, session->float_value);
                            BACK_UP;
                            session->state = PARSING_EXPRESSION;
                        }
                        break;
                }
                break;

            case PARSING_DECIMAL_FRACTION:
                switch (ch) {
                    case 'e':
                    case 'E':
                        session->state = PARSING_DECIMAL_EXPONENT_SIGN;
                        break;

                    default:
                        if (ch >= '0' && ch <= '9')
                            add_float_fraction_digit(session, ch - '0', 10);
                        else {
                            EMIT(float_literal, session->float_value);
                            BACK_UP;
                            session->state = PARSING_EXPRESSION;
                        }
                        break;
                }
                break;

            case PARSING_DECIMAL_EXPONENT_SIGN:
                if (ch == '-')
                    session->exponent_sign = -1;
                else {
                    session->exponent_sign = 1;
                    BACK_UP;
                }
                session->integer_value = 0;
                session->digit_count = 0;
                session->state = PARSING_DECIMAL_EXPONENT;
                break;

            case PARSING_DECIMAL_EXPONENT:
                if (ch >= '0' && ch <= '9') {
                    if (session->digit_count == 3) {
                        ERROR("Decimal exponent must be 3 digits or less");
                    } else
                        add_safe_digit(session, ch - '0', 10);
                } else if (!session->digit_count) {
                    ERROR("No digits after decimal exponent delimiter");
                } else {
                    session->float_value *= powf(10, session->integer_value);
                    EMIT(float_literal, session->float_value);
                    BACK_UP;
                }
                break;

            case PARSING_HEX_FLOAT:
                switch (ch) {
                    case '.':
                        session->state = PARSING_HEX_FRACTION;
                        session->digit_count = 0;
                        break;

                    case 'p':
                    case 'P':
                        session->state = PARSING_HEX_EXPONENT_SIGN;
                        break;

                    default:
                        digit_value = hex_digit_to_int(ch);
                        if (digit_value != -1)
                            add_float_digit(session, digit_value, 16);
                        else {
                            EMIT(float_literal, session->float_value);
                            BACK_UP;
                            session->state = PARSING_EXPRESSION;
                        }
                        break;
                }
                break;

            case PARSING_HEX_FRACTION:
                switch (ch) {
                    case 'p':
                    case 'P':
                        session->state = PARSING_HEX_EXPONENT_SIGN;
                        break;

                    default:
                        digit_value = hex_digit_to_int(ch);
                        if (digit_value  != -1)
                            add_float_fraction_digit(session, digit_value, 16);
                        else {
                            EMIT(float_literal, session->float_value);
                            BACK_UP;
                            session->state = PARSING_EXPRESSION;
                        }
                        break;
                }
                break;

            case PARSING_HEX_EXPONENT_SIGN:
                if (ch == '-')
                    session->exponent_sign = -1;
                else {
                    session->exponent_sign = 1;
                    BACK_UP;
                }
                session->integer_value = 0;
                session->digit_count = 0;
                session->state = PARSING_HEX_EXPONENT;
                break;

            case PARSING_HEX_EXPONENT:
                digit_value = hex_digit_to_int(ch);
                if (digit_value != -1) {
                    if (session->digit_count == 2) {
                        ERROR("Hex exponent must be 2 digits or less");
                    } else
                        add_safe_digit(session, digit_value, 16);
                } else if (!session->digit_count) {
                    ERROR("No digits after hex exponent delimiter");
                } else {
                    session->float_value *= powf(16, session->integer_value);
                    EMIT(float_literal, session->float_value);
                    BACK_UP;
                }
                break;

            default:
                ERROR("Internal: Unknown lexer state %d", session->state);
                break;
        }
}

void gcode_lexer_finish(GCodeLexerSession* session) {
    if (session->state != PARSING_COMPLETE)
        gcode_lexer_lex(session, "\0", 1);
    switch (session->state) {
        case PARSING_COMPLETE:
        case PARSING_WHITESPACE:
            break;

        case PARSING_STRING:
            session->error(session->context, "Unterminated string literal");
            break;

        case PARSING_EXPRESSION:
            session->error(session->context, "Unterminated expression");
            break;

        default:
            session->error(session->context,
                "Internal error: Lexing terminated in unknown state %d",
                session->state);
    }
    free(session->token_str);
    free(session);
}
