#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "debug.h"
#include "table.h"

static parse_rule *get_rule(token_type type);
static void expression(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign);
static void grouping(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign);
static void binary(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign);
static void statement(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign);
static void declaration(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign);
static uint8_t identifier_constant(VM* vm, parser* parser, chunk* chunk, token* name);
static int resolve_local(parser* parser, compiler* compiler, token* name);

static void error_at(parser *parser, token *token, const char *message)
{
    if (parser->panic_mode)
        return;
    parser->panic_mode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
        fprintf(stderr, " at end");
    else if (token->type != TOKEN_ERROR)
        fprintf(stderr, " at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

static void error(parser *parser, const char *message)
{
    error_at(parser, &parser->previous, message);
}

static void error_at_current(parser *parser, const char *message)
{
    error_at(parser, &parser->current, message);
}

obj_string* allocate_string(VM* vm, char* chars, int length, uint32_t hash)
{
    struct Obj* obj = (struct Obj*)malloc(sizeof(obj_string));
    obj->type = OBJ_STRING;
    obj->next = vm->objects;
    vm->objects = obj;
    obj_string* str = (obj_string*)obj;
    str->length = length;
    str->chars = chars;
    str->hash = hash;

    table_set(&vm->strings, str, NIL_VAL);

    return str;
}

uint32_t hash_string(const char* key, int length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

obj_string* copy_string(VM* vm, char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    obj_string* interned = table_find_string(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        return interned;
    }

    return allocate_string(vm, chars, length, hash);
}

static void advance(parser *parser, scanner *scanner)
{
    parser->previous = parser->current;

    for (;;)
    {
        parser->current = scan_token(scanner);
        if (parser->current.type != TOKEN_ERROR)
            break;

        error_at_current(parser, parser->current.start);
    }
}

static void consume(parser *parser, scanner *scanner, token_type type, const char *message)
{
    if (parser->current.type == type)
    {
        advance(parser, scanner);
        return;
    }

    error_at_current(parser, message);
}

static bool check(parser* parser, token_type type)
{
    return parser->current.type == type;
}

static bool match(parser* parser, scanner* scanner, token_type type)
{
    if (!check(parser, type)) return false;
    advance(parser, scanner);
    return true;
}

static void emit_byte(parser *parser, chunk *chunk, uint8_t byte)
{
    write_chunk(chunk, byte, parser->previous.line);
}

static void emit_bytes(parser *parser, chunk *chunk, uint8_t byte1, uint8_t byte2)
{
    emit_byte(parser, chunk, byte1);
    emit_byte(parser, chunk, byte2);
}

static void emit_return(parser *parser, chunk *chunk)
{
    emit_byte(parser, chunk, OP_RETURN);
}

static uint8_t make_constant(parser *parser, chunk *chunk, Value value)
{
    int constant = add_constant(chunk, value);
    if (constant > UINT8_MAX)
    {
        error(parser, "Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(parser *parser, chunk *chunk, Value value)
{
    emit_bytes(parser, chunk, OP_CONSTANT, make_constant(parser, chunk, value));
}

static void number(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign)
{
    double value = strtod(parser->previous.start, NULL);
    emit_constant(parser, chunk, NUMBER_VAL(value));
}

static void string(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    emit_constant(parser, chunk, OBJ_VAL(copy_string(vm, parser->previous.start + 1, parser->previous.length - 2)));
}

static void named_variable(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, token name, bool can_assign)
{
    uint8_t get_op, set_op;
    int arg = resolve_local(parser, compiler, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else {
        arg = identifier_constant(vm, parser, chunk, &name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(vm, parser, scanner, chunk, compiler, can_assign);
        emit_bytes(parser, chunk, set_op, (uint8_t)arg);
    } else {
        emit_bytes(parser, chunk, get_op, (uint8_t)arg);
    }
}

static void variable(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    named_variable(vm, parser, scanner, chunk, compiler, parser->previous, can_assign);
}

static void parse_precedence(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, precedence prec)
{
    advance(parser, scanner);
    parse_fn prefix_rule = get_rule(parser->previous.type)->prefix;
    if (!prefix_rule)
    {
        error(parser, "Expect expression");
        return;
    }

    bool can_assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(vm, parser, scanner, chunk, compiler, can_assign);

    while (prec <= get_rule(parser->current.type)->prec)
    {
        advance(parser, scanner);
        parse_fn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(vm, parser, scanner, chunk, compiler, can_assign);
    }

    if (can_assign && match(parser, scanner, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target");
    }
}

static uint8_t identifier_constant(VM* vm, parser* parser, chunk* chunk, token* name)
{
    return make_constant(parser, chunk, OBJ_VAL(copy_string(vm, name->start, name->length)));
}

static bool identifiers_equal(token* a, token* b)
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(parser* parser, compiler* compiler, token* name)
{
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error(parser, "Can't read local variable in its own initializer");
            }
            return i;
        }
    }

    return -1;
}

static void add_local(parser* parser, compiler* compiler, token name)
{
    if (compiler->local_count == UINT8_COUNT) {
        error(parser, "Too many local variables in function");
        return;
    }

    Local* local = &compiler->locals[compiler->local_count++];
    local->name = name;
    local->depth = -1;
}

static void declare_variable(parser* parser, compiler* compiler)
{
    if (compiler->scope_depth == 0) return;

    token* name = &parser->previous;
    for (int i = compiler->local_count - 1; i>= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error(parser, "Already variable with this name in this scope");
        }
    }

    add_local(parser, compiler, *name);
}

static uint8_t parse_variable(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, const char* message)
{
    consume(parser, scanner, TOKEN_IDENTIFIER, message);

    declare_variable(parser, compiler);
    if (compiler->scope_depth > 0) return 0;

    return identifier_constant(vm, parser, chunk, &parser->previous);
}

static void unary(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign)
{
    token_type operator_type = parser->previous.type;

    parse_precedence(vm, parser, scanner, chunk, compiler, PREC_UNARY);

    switch (operator_type)
    {
    case TOKEN_BANG:
        emit_byte(parser, chunk, OP_NOT);
        break;
    case TOKEN_MINUS:
        emit_byte(parser, chunk, OP_NEGATE);
        break;
    default:
        return;
    }
}

static void end_compiler(parser *parser, chunk *chunk)
{
    emit_return(parser, chunk);
#ifdef DEBUG
    if (!parser->had_error)
        disassemble_chunk(chunk, "code");
#endif
}

static void begin_scope(compiler* compiler)
{
    compiler->scope_depth++;
}

static void end_scope(parser* parser, chunk* chunk, compiler* compiler)
{
    compiler->scope_depth--;

    while (compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
        emit_byte(parser, chunk, OP_POP);
        compiler->local_count--;
    }
}

static void literal(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign)
{
    switch (parser->previous.type)
    {
    case TOKEN_FALSE:
        emit_byte(parser, chunk, OP_FALSE);
        break;
    case TOKEN_NIL:
        emit_byte(parser, chunk, OP_NIL);
        break;
    case TOKEN_TRUE:
        emit_byte(parser, chunk, OP_TRUE);
        break;
    default:
        return;
    }
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
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static parse_rule *get_rule(token_type type)
{
    return &rules[type];
}

static void binary(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign)
{
    token_type operator_type = parser->previous.type;
    parse_rule *rule = get_rule(operator_type);
    parse_precedence(vm, parser, scanner, chunk, compiler, (precedence)(rule->prec + 1));

    switch (operator_type)
    {
    case TOKEN_BANG_EQUAL:
        emit_bytes(parser, chunk, OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emit_byte(parser, chunk, OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emit_byte(parser, chunk, OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emit_bytes(parser, chunk, OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emit_byte(parser, chunk, OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emit_bytes(parser, chunk, OP_GREATER, OP_NOT);
        break;
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

static void grouping(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign)
{
    expression(vm, parser, scanner, chunk, compiler, can_assign);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void expression(VM* vm, parser *parser, scanner *scanner, chunk *chunk, compiler* compiler, bool can_assign)
{
    parse_precedence(vm, parser, scanner, chunk, compiler, PREC_ASSIGNMENT);
}

static void block(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(vm, parser, scanner, chunk, compiler, can_assign);
    }

    consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void mark_initialized(compiler* compiler)
{
    compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

static void define_variable(parser* parser, chunk* chunk, compiler* compiler, uint8_t global)
{
    if (compiler->scope_depth > 0) {
        mark_initialized(compiler);
        return;
    }
    emit_bytes(parser, chunk, OP_DEFINE_GLOBAL, global);
}

static void var_declaration(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    uint8_t global = parse_variable(vm, parser, scanner, chunk, compiler, "Expect variable name");

    if (match(parser, scanner, TOKEN_EQUAL)) {
        expression(vm, parser, scanner, chunk, compiler, can_assign);
    } else {
        emit_byte(parser, chunk, OP_NIL);
    }
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    define_variable(parser, chunk, compiler, global);
}

static void expression_statement(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    expression(vm, parser, scanner, chunk, compiler, can_assign);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression");
    emit_byte(parser, chunk, OP_POP);
}

static void print_statement(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    expression(vm, parser, scanner, chunk, compiler, can_assign);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after value");
    emit_byte(parser, chunk, OP_PRINT);
}

static void synchronize(parser* parser, scanner* scanner)
{
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;

        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ;
        }

        advance(parser, scanner);
    }
}

static void declaration(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    if (match(parser, scanner, TOKEN_VAR)) {
        var_declaration(vm, parser, scanner, chunk, compiler, can_assign);
    } else {
        statement(vm, parser, scanner, chunk, compiler, can_assign);
    }

    if (parser->panic_mode) synchronize(parser, scanner);
}

static void statement(VM* vm, parser* parser, scanner* scanner, chunk* chunk, compiler* compiler, bool can_assign)
{
    if (match(parser, scanner, TOKEN_PRINT)) {
        print_statement(vm, parser, scanner, chunk, compiler, can_assign);
    } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
        begin_scope(compiler);
        block(vm, parser, scanner, chunk, compiler, can_assign);
        end_scope(parser, chunk, compiler);
    } else {
        expression_statement(vm, parser, scanner, chunk, compiler, can_assign);
    }
}

bool compile(char *source, chunk *ch, VM* vm)
{
    scanner *scanner = init_scanner(source);
    compiler* c = (compiler*)calloc(1, sizeof(compiler));
    parser *p = (parser *)calloc(1, sizeof(parser));
    chunk *chunk = ch;
    bool error;

    advance(p, scanner);
    
    while (!match(p, scanner, TOKEN_EOF)) {
        declaration(vm, p, scanner, chunk, c, false);
    }

    end_compiler(p, chunk);

    error = !p->had_error;
    free(scanner);
    free(p);
    return error;
}