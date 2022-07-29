#include "common.h"
#include "scanner.h"

scanner* init_scanner(const char* source)
{
    scanner* s = (scanner*)malloc(sizeof(scanner));
    s->start = source;
    s->current = source;
    s->line = 1;
    return s;
}

static char peek_next(scanner* s)
{
    if (*s->current == '\0')
        return '\0';
    return s->current[1];
}

static token make_token(scanner* s, token_type type)
{
    token t;
    t.type = type;
    t.start = s->start;
    t.length = (int)(s->current - s->start);
    t.line = s->line;

    return t;
}

static token_type check_keyword(scanner* s, int start, int length, const char* rest, token_type type)
{
    if (s->current - s->start == start + length && memcmp(s->start + start, rest, length) == 0)
        return type;

    return TOKEN_IDENTIFIER;
}

static token_type identifier_type(scanner* s)
{
    switch (s->start[0]) {
        case 'a': return check_keyword(s, 1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(s, 1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(s, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (s->current - s->start > 1){
                switch (s->start[1]) {
                    case 'a': return check_keyword(s, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(s, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(s, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return check_keyword(s, 1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(s, 1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(s, 1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(s, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(s, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(s, 1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'h': return check_keyword(s, 2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(s, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(s, 1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(s, 1, 4, "hile", TOKEN_WHILE);
        }
    return TOKEN_IDENTIFIER;
}

static token identifier(scanner* s)
{
    while (is_alpha(*s->current ) || is_digit(*s->current )) s->current++;

    return make_token(s, identifier_type(s));
}

static token number(scanner* s)
{
    while (is_digit(*s->current)) s->current++;;

    if (*s->current == '.' && is_digit(peek_next(s))) {
        s->current++;
        while(is_digit(*s->current)) s->current++;
    }

    return make_token(s, TOKEN_NUMBER);
}

static token error_token(scanner* s, const char* message)
{
    token t;
    t.type = TOKEN_ERROR;
    t.start = message;
    t.length = (int)strlen(message);
    t.line = s->line;

    return t;
}

static bool match(scanner* s, char expected)
{
    if (*s->current) return false;
    if (*s->current != expected) return false;

    *s->current++;
    return true;
}

static void skip_whitespace(scanner* s)
{
    for(;;) { 
        char c = *s->current;
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                s->current++;
                break;
            case '\n':
                s->line++;
                s->current++;
                break;
            case '/':
                if (peek_next(s) == '/') {
                    while (*s->current != '\n' && !(*s->current == '\0'))
                        s->current++;
                } else {
                    return;
                }
                break;
            default:
                return;
                break;
        }
    }
}

static token string(scanner* s)
{
    while (*s->current != '"' && !(*s->current == '\0')) {
        if (*s->current == '\n') s->line++;
        s->current++;
    }

    if (*s->current == '\0') return error_token(s, "Unterminated string");

    s->current++;
    return make_token(s, TOKEN_STRING);
}

token scan_token(scanner* s)
{
    skip_whitespace(s);
    s->start = s->current;

    if(*s->current == '\0') 
        return make_token(s, TOKEN_EOF);

    s->current++;
    char c = s->current[-1];

    if(is_alpha(c)) return identifier(s);
    if (is_digit(c)) return number(s);

    switch (c) {
    case '(': return make_token(s, TOKEN_LEFT_PAREN);
    case ')': return make_token(s, TOKEN_RIGHT_PAREN);
    case '{': return make_token(s, TOKEN_LEFT_BRACE);
    case '}': return make_token(s, TOKEN_RIGHT_BRACE);
    case ';': return make_token(s, TOKEN_SEMICOLON);
    case ',': return make_token(s, TOKEN_COMMA);
    case '.': return make_token(s, TOKEN_DOT);
    case '-': return make_token(s, TOKEN_MINUS);
    case '+': return make_token(s, TOKEN_PLUS);
    case '/': return make_token(s, TOKEN_SLASH);
    case '*': return make_token(s, TOKEN_STAR);
    case '!': return make_token(s, match(s, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return make_token(s, match(s, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return make_token(s, match(s, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return make_token(s, match(s, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string(s);
    }

    return error_token(s, "Unexpected character.");
}