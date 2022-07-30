#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "debug.h"

static parse_rule* get_rule(token_type type);
static void expression(parser* parser, scanner* scanner, chunk* chunk);
static void grouping(parser* parser, scanner* scanner, chunk* chunk);
static void binary(parser* parser, scanner* scanner, chunk* chunk);

static void error_at(parser* parser, token* token, const char* message)
{
    if (parser->panic_mode) return;
    parser->panic_mode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) fprintf(stderr, " at end");
    else if (token->type != TOKEN_ERROR) fprintf(stderr, " at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

static void error(parser* parser, const char* message)
{
    error_at(parser, &parser->previous, message);
}

static void error_at_current(parser* parser, const char* message)
{
    error_at(parser, &parser->current, message);
}

static void advance(parser* parser, scanner* scanner)
{
    parser->previous = parser->current;

    for(;;) {
        parser->current = scan_token(scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        error_at_current(parser, parser->current.start);
    }
}

static void consume(parser* parser, scanner* scanner, token_type type, const char* message)
{
    if (parser->current.type == type) {
        advance(parser, scanner);
        return;
    }

    error_at_current(parser, message);
}

static void emit_byte(parser* parser, chunk* chunk, uint8_t byte)
{
    write_chunk(chunk, byte, parser->previous.line);
}

static void emit_bytes(parser* parser, chunk* chunk, uint8_t byte1, uint8_t byte2)
{
    emit_byte(parser, chunk, byte1);
    emit_byte(parser, chunk, byte2);
}

static void emit_return(parser* parser, chunk* chunk)
{
    emit_byte(parser, chunk, OP_RETURN);
}

static uint8_t make_constant(parser* parser, chunk* chunk, Value value)
{
    int constant = add_constant(chunk, value);
    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(parser* parser, chunk* chunk, Value value)
{
    emit_bytes(parser, chunk, OP_CONSTANT, make_constant(parser, chunk, value));
}

static void number(parser* parser, scanner* scanner, chunk* chunk)
{
    double value = strtod(parser->previous.start, NULL);
    emit_constant(parser, chunk, value);
}

static void parse_precedence(parser* parser, scanner* scanner, chunk* chunk, precedence prec)
{
    advance(parser, scanner);
    parse_fn prefix_rule = get_rule(parser->previous.type)->prefix;
    if (!prefix_rule) {
        error(parser, "Expect expression");
        return;
    }

    prefix_rule(parser, scanner, chunk);

    while(prec <= get_rule(parser->current.type)->prec) {
        advance(parser, scanner);
        parse_fn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(parser, scanner, chunk);
    }
}

static void unary(parser* parser, scanner* scanner, chunk* chunk)
{
    token_type operator_type = parser->previous.type;

    parse_precedence(parser, scanner, chunk, PREC_UNARY);

    switch (operator_type) {
        case TOKEN_MINUS: 
            emit_byte(parser, chunk, OP_NEGATE);
            break;
        default:
            return;
    }
}

static void end_compiler(parser* parser, chunk* chunk)
{
    emit_return(parser, chunk);
#ifdef DEBUG
    if (!parser->had_error) disassemble_chunk(chunk, "code");
#endif
}

parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static parse_rule* get_rule(token_type type)
{
    return &rules[type];
}

static void binary(parser* parser, scanner* scanner, chunk* chunk)
{
    token_type operator_type = parser->previous.type;
    parse_rule* rule = get_rule(operator_type);
    parse_precedence(parser, scanner, chunk, (precedence)(rule->prec + 1));

    switch(operator_type) {
        case TOKEN_PLUS:
            emit_byte(parser, chunk, OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_byte(parser, chunk, OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emit_byte(parser, chunk, OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emit_byte(parser, chunk, OP_DIVIDE);
            break;
        default:
            return;
    }
}

static void grouping(parser* parser, scanner* scanner, chunk* chunk)
{
    expression(parser, scanner, chunk);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void expression(parser* parser, scanner* scanner, chunk* chunk)
{
    parse_precedence(parser, scanner, chunk, PREC_ASSIGNMENT);
}

bool compile(const char* source, chunk* ch)
{
    scanner* scanner = init_scanner(source);
    parser* p = (parser*)calloc(1, sizeof(parser));
    chunk* chunk = ch;
    bool error;
    
    advance(p, scanner);
    expression(p, scanner, chunk);
    consume(p, scanner, TOKEN_EOF, "Expect end of expression");
    end_compiler(p, chunk);

    error = !p->had_error;
    free(scanner);
    free(p);
    return error;
}