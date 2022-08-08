#include "interpreter.h"

/* COMPILER OPERATIONS */
static parse_rule *get_rule(token_type type);
static void expression(polity_interpreter* interpreter);
static void grouping(polity_interpreter* interpreter);
static void binary(polity_interpreter* interpreter);
static void statement(polity_interpreter* interpreter);
static void declaration(polity_interpreter* interpreter);
static uint8_t identifier_constant(polity_interpreter* interpreter, token* name);
static int resolve_local(polity_interpreter* interpreter, token* name);
static void and_(polity_interpreter* interpreter);

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

static void advance(polity_interpreter* interpreter)
{
    parser* parser = interpreter->parser;
    parser->previous = parser->current;

    while (1) {
        parser->current = scan_token(interpreter->scanner);
        if (parser->current.type != TOKEN_ERROR)
            break;

        error_at(parser, &parser->current, parser->current.start);
    }
}

static void consume(polity_interpreter* interpreter, token_type type, const char *message)
{
    if (interpreter->parser->current.type == type) {
        advance(interpreter);
        return;
    }
    error_at(interpreter->parser, &interpreter->parser->previous, interpreter->parser->current.start);
}

static bool match(polity_interpreter* interpreter, token_type type)
{
    if (!(interpreter->parser->current.type == type))
        return false;

    advance(interpreter);
    return true;
}

static void emit_byte(polity_interpreter* interpreter, uint8_t byte)
{
    write_chunk(interpreter->chunk, byte, interpreter->parser->previous.line);
}

static void emit_bytes(polity_interpreter* interpreter, uint8_t byte1, uint8_t byte2)
{
    emit_byte(interpreter, byte1);
    emit_byte(interpreter, byte2);
}

static int emit_jump(polity_interpreter* interpreter, uint8_t instruction)
{
    emit_byte(interpreter, instruction);
    emit_byte(interpreter, 0xFF);
    emit_byte(interpreter, 0xFF);
    return interpreter->chunk->count - 2;
}

static void emit_loop(polity_interpreter* interpreter, int loop_start)
{
    emit_byte(interpreter, OP_LOOP);

    int offset = interpreter->chunk->count - loop_start + 2;
    if (offset > UINT16_MAX)
        error(interpreter->parser, "Loop body too large");

    emit_byte(interpreter, (offset >> 8) & 0xFF);
    emit_byte(interpreter, offset & 0xFF);
}

static void patch_jump(polity_interpreter* interpreter, int offset)
{
    chunk* chunk = interpreter->chunk;
    int jump = chunk->count - offset - 2;

    if (jump > UINT16_MAX)
        error(interpreter->parser, "Too much code to jump over");

    chunk->code[offset] = (jump >> 8) & 0xFF;
    chunk->code[offset + 1] = jump & 0xFF;
}

static uint8_t make_constant(polity_interpreter* interpreter, value val)
{
    int constant = add_constant(interpreter->chunk, val);
    if (constant > UINT8_MAX) {
        error(interpreter->parser, "Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(polity_interpreter* interpreter, value val)
{
    emit_bytes(interpreter, OP_CONSTANT, make_constant(interpreter, val));
}

static void number(polity_interpreter* interpreter)
{
    emit_constant(interpreter,
            NUMBER_VAL(strtod(interpreter->parser->previous.start, NULL)));
}

static void parse_precedence(polity_interpreter* interpreter, precedence prec)
{
    parser* parser = interpreter->parser;
    advance(interpreter);
    parse_fn prefix_rule = get_rule(parser->previous.type)->prefix;
    if (!prefix_rule) {
        error(parser, "Expect expression");
        return;
    }

    interpreter->can_assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(interpreter);

    while (prec <= get_rule(parser->current.type)->prec) {
        advance(interpreter);
        parse_fn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(interpreter);
    }

    if (interpreter->can_assign && match(interpreter, TOKEN_EQUAL))
        error(parser, "Invalid assignment target");
}

static void or_(polity_interpreter* interpreter)
{
    int else_jump = emit_jump(interpreter, OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(interpreter, OP_JUMP);

    patch_jump(interpreter, else_jump);
    emit_byte(interpreter, OP_POP);

    parse_precedence(interpreter, PREC_OR);
    patch_jump(interpreter, end_jump);
}

static void string(polity_interpreter* interpreter)
{
    emit_constant(interpreter,
        OBJ_VAL(copy_string(interpreter->vm, interpreter->parser->previous.start + 1, interpreter->parser->previous.length - 2)));
}

static void named_variable(polity_interpreter* interpreter, token name)
{
    uint8_t get_op, set_op;
    int arg = resolve_local(interpreter, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else {
        arg = identifier_constant(interpreter, &name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (interpreter->can_assign && match(interpreter, TOKEN_EQUAL)) {
        expression(interpreter);
        emit_bytes(interpreter, set_op, (uint8_t)arg);
    } else
        emit_bytes(interpreter, get_op, (uint8_t)arg);
}

static void variable(polity_interpreter* interpreter)
{
    named_variable(interpreter, interpreter->parser->previous);
}

static uint8_t identifier_constant(polity_interpreter* interpreter, token* name)
{
    return make_constant(interpreter, OBJ_VAL(copy_string(interpreter->vm, name->start, name->length)));
}

static bool identifiers_equal(token* a, token* b)
{
    if (a->length != b->length)
        return false;

    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(polity_interpreter* interpreter, token* name)
{
    for (int i = interpreter->compiler->local_count - 1; i >= 0; i--) {
        local* local = &interpreter->compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1)
                error(interpreter->parser, "Can't read local variable in its own initializer");

            return i;
        }
    }

    return -1;
}

static void add_local(polity_interpreter* interpreter, token name)
{
    if (interpreter->compiler->local_count == UINT8_COUNT) {
        error(interpreter->parser, "Too many local variables in function");
        return;
    }

    local* local = &interpreter->compiler->locals[interpreter->compiler->local_count++];
    local->name = name;
    local->depth = -1;
}

static void declare_variable(polity_interpreter* interpreter)
{
    parser* parser = interpreter->parser;
    compiler* compiler = interpreter->compiler;

    if (compiler->scope_depth == 0)
        return;

    token* name = &parser->previous;
    for (int i = compiler->local_count - 1; i>= 0; i--) {
        local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scope_depth)
            break;

        if (identifiers_equal(name, &local->name))
            error(parser, "Already variable with this name in this scope");
    }

    add_local(interpreter, *name);
}

static uint8_t parse_variable(polity_interpreter* interpreter, const char* message)
{
    consume(interpreter, TOKEN_IDENTIFIER, message);

    declare_variable(interpreter);
    if (interpreter->compiler->scope_depth > 0)
        return 0;

    return identifier_constant(interpreter, &interpreter->parser->previous);
}

static void unary(polity_interpreter* interpreter)
{
    token_type operator_type = interpreter->parser->previous.type;

    parse_precedence(interpreter, PREC_UNARY);

    switch (operator_type)
    {
    case TOKEN_BANG:
        emit_byte(interpreter, OP_NOT);
        break;
    case TOKEN_MINUS:
        emit_byte(interpreter, OP_NEGATE);
        break;
    default:
        return;
    }
}

static void end_compiler(polity_interpreter* interpreter)
{
    emit_byte(interpreter, OP_RETURN); /* Emit return */
#ifdef DEBUG
    if (!interpreter->parser->had_error)
        disassemble_chunk(interpreter->chunk, "code");
#endif
}

static void begin_scope(compiler* compiler)
{
    compiler->scope_depth++;
}

static void end_scope(polity_interpreter* interpreter)
{
    compiler* compiler = interpreter->compiler;
    compiler->scope_depth--;

    while (compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
        emit_byte(interpreter, OP_POP);
        compiler->local_count--;
    }
}

static void literal(polity_interpreter* interpreter)
{
    switch (interpreter->parser->previous.type)
    {
    case TOKEN_FALSE:
        emit_byte(interpreter, OP_FALSE);
        break;
    case TOKEN_NIL:
        emit_byte(interpreter, OP_NIL);
        break;
    case TOKEN_TRUE:
        emit_byte(interpreter, OP_TRUE);
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
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
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

static void binary(polity_interpreter* interpreter)
{
    token_type operator_type = interpreter->parser->previous.type;
    parse_rule *rule = get_rule(operator_type);
    parse_precedence(interpreter, (precedence)(rule->prec + 1));

    switch (operator_type)
    {
    case TOKEN_BANG_EQUAL:
        emit_bytes(interpreter, OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emit_byte(interpreter, OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emit_byte(interpreter, OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emit_bytes(interpreter, OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emit_byte(interpreter, OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emit_bytes(interpreter, OP_GREATER, OP_NOT);
        break;
    case TOKEN_PLUS:
        emit_byte(interpreter, OP_ADD);
        break;
    case TOKEN_MINUS:
        emit_byte(interpreter, OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emit_byte(interpreter, OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emit_byte(interpreter, OP_DIVIDE);
        break;
    default:
        return;
    }
}

static void grouping(polity_interpreter* interpreter)
{
    expression(interpreter);
    consume(interpreter, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void expression(polity_interpreter* interpreter)
{
    parse_precedence(interpreter, PREC_ASSIGNMENT);
}

static void block(polity_interpreter* interpreter)
{
    while (!(interpreter->parser->current.type == TOKEN_RIGHT_BRACE) && !(interpreter->parser->current.type == TOKEN_EOF)) {
        declaration(interpreter);
    }

    consume(interpreter, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void mark_initialized(compiler* compiler)
{
    compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

static void define_variable(polity_interpreter* interpreter, uint8_t global)
{
    if (interpreter->compiler->scope_depth > 0) {
        mark_initialized(interpreter->compiler);
        return;
    }
    emit_bytes(interpreter, OP_DEFINE_GLOBAL, global);
}

static void and_(polity_interpreter* interpreter)
{
    int end_jump = emit_jump(interpreter, OP_JUMP_IF_FALSE);

    emit_byte(interpreter, OP_POP);
    parse_precedence(interpreter, PREC_AND);

    patch_jump(interpreter, end_jump);
}

static void var_declaration(polity_interpreter* interpreter)
{
    uint8_t global = parse_variable(interpreter, "Expect variable name");

    if (match(interpreter, TOKEN_EQUAL))
        expression(interpreter);
    else
        emit_byte(interpreter, OP_NIL);

    consume(interpreter, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    define_variable(interpreter, global);
}

static void expression_statement(polity_interpreter* interpreter)
{
    expression(interpreter);
    consume(interpreter, TOKEN_SEMICOLON, "Expect ';' after expression");
    emit_byte(interpreter, OP_POP);
}

static void for_statement(polity_interpreter* interpreter)
{
    begin_scope(interpreter->compiler);
    consume(interpreter, TOKEN_LEFT_PAREN, "Expect '(' after 'for'");
    if (match(interpreter, TOKEN_SEMICOLON)) {
        /* No initializer */
    } else if (match(interpreter, TOKEN_VAR))
        var_declaration(interpreter);
    else
        expression_statement(interpreter);

    int loop_start = interpreter->chunk->count;

    int exit_jump = -1;
    if (!match(interpreter, TOKEN_SEMICOLON)) {
        expression(interpreter);
        consume(interpreter, TOKEN_SEMICOLON, "Expect ';' after loop condition");

        /* Jump out of the loop if the condition is false */
        exit_jump = emit_jump(interpreter, OP_JUMP_IF_FALSE);
        emit_byte(interpreter, OP_POP);
    }
    
    if (!match(interpreter, TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(interpreter, OP_JUMP);

        int increment_start = interpreter->chunk->count;
        expression(interpreter);
        emit_byte(interpreter, OP_POP);
        consume(interpreter, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");

        emit_loop(interpreter, loop_start);
        loop_start = increment_start;
        patch_jump(interpreter, body_jump);
    }

    statement(interpreter);

    emit_loop(interpreter, loop_start);

    if (exit_jump != -1) {
        patch_jump(interpreter, exit_jump);
        emit_byte(interpreter, OP_POP);
    }

    end_scope(interpreter);
}

static void while_statement(polity_interpreter* interpreter)
{
    int loop_start = interpreter->chunk->count;

    consume(interpreter, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
    expression(interpreter);
    consume(interpreter, TOKEN_RIGHT_PAREN, "Expect ')' after 'while'");

    int exit_jump = emit_jump(interpreter, OP_JUMP_IF_FALSE);

    emit_byte(interpreter, OP_POP);
    statement(interpreter);

    emit_loop(interpreter, loop_start);

    patch_jump(interpreter, exit_jump);
    emit_byte(interpreter, OP_POP);
}

static void print_statement(polity_interpreter* interpreter)
{
    expression(interpreter);
    consume(interpreter, TOKEN_SEMICOLON, "Expect ';' after value");
    emit_byte(interpreter, OP_PRINT);
}

static void synchronize(polity_interpreter* interpreter)
{
    parser* parser = interpreter->parser;
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON)
            return;

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

        advance(interpreter);
    }
}

static void declaration(polity_interpreter* interpreter)
{
    if (match(interpreter, TOKEN_VAR))
        var_declaration(interpreter);
    else
        statement(interpreter);

    if (interpreter->parser->panic_mode)
        synchronize(interpreter);
}

static void if_statement(polity_interpreter* interpreter)
{
    consume(interpreter, TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
    expression(interpreter);
    consume(interpreter, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    int then_jump = emit_jump(interpreter, OP_JUMP_IF_FALSE);
    emit_byte(interpreter, OP_POP);
    statement(interpreter);

    int else_jump = emit_jump(interpreter, OP_JUMP);

    patch_jump(interpreter, then_jump);
    emit_byte(interpreter, OP_POP);

    if (match(interpreter, TOKEN_ELSE))
        statement(interpreter);

    patch_jump(interpreter, else_jump);
}

static void statement(polity_interpreter* interpreter)
{
    if (match(interpreter, TOKEN_PRINT))
        print_statement(interpreter);
    else if (match(interpreter, TOKEN_FOR))
        for_statement(interpreter);
    else if (match(interpreter, TOKEN_IF))
        if_statement(interpreter);
    else if (match(interpreter, TOKEN_WHILE))
        while_statement(interpreter);
    else if (match(interpreter, TOKEN_LEFT_BRACE)) {
        begin_scope(interpreter->compiler);
        block(interpreter);
        end_scope(interpreter);
    } else
        expression_statement(interpreter);

}

bool compile(char *source, polity_interpreter* interpreter)
{
    interpreter->scanner = init_scanner(source);
    interpreter->compiler = (compiler*)calloc(1, sizeof(compiler));
    interpreter->parser = (parser *)calloc(1, sizeof(parser));

    advance(interpreter);
    
    while (!match(interpreter, TOKEN_EOF))
        declaration(interpreter);

    end_compiler(interpreter);

    interpreter->can_assign = !interpreter->parser->had_error;
    free(interpreter->scanner);
    free(interpreter->parser);
    free(interpreter->compiler);
    return interpreter->can_assign;
}

/* CHUNK OPERATIONS */
void write_chunk(chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1) {
        chunk->capacity = (chunk->capacity) < 8 ? 8 : (chunk->capacity) * 2;
        chunk->code = (uint8_t*)realloc(chunk->code, sizeof(uint8_t) * (chunk->capacity));
        chunk->lines = (int*)realloc(chunk->lines, sizeof(int) * (chunk->capacity));
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void free_chunk(chunk* chunk)
{
    free(chunk->code);
    free(chunk->lines);
    free(chunk->constants.values);
    free(chunk);
}

int add_constant(chunk* chunk, value val)
{
    if (chunk->constants.capacity < chunk->constants.count + 1) {
        chunk->constants.capacity = (chunk->constants.capacity) < 8 ? 8 : (chunk->constants.capacity) * 2;
        chunk->constants.values = (value*)realloc(chunk->constants.values, sizeof(value) * (chunk->constants.capacity));
    }

    chunk->constants.values[chunk->constants.count++] = val;
    return chunk->constants.count - 1;
}

/* TABLE OPERATIONS */  
static entry* find_entry(entry* entries, int capacity, obj_string* key)
{
    uint32_t index = key->hash % capacity;
    entry* tombstone = NULL;
    for (;;) {
        entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool table_get(table* table, obj_string* key, value* val)
{
    if (table->count == 0) return false;

    entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *val = entry->value;
    return true;
}

static void adjust_capacity(table* table, int capacity)
{
    entry* entries_old = table->entries;
    entry* entries = (entry*)malloc(sizeof(entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        entry* table_entry = &table->entries[i];
        if (table_entry->key == NULL) continue;

        entry* dest = find_entry(entries, capacity, table_entry->key);
        dest->key = table_entry->key;
        dest->value = table_entry->value;
        table->count++;
    }

    table->entries = entries;
    table->capacity = capacity;
    free(entries_old);
}

bool table_set(table* table, obj_string* key, value val)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = ((table->capacity) < 8 ? 8 : (table->capacity) * 2);
        adjust_capacity(table, capacity);
    }

    entry* entry = find_entry(table->entries, table->capacity, key);

    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = val;
    return is_new_key;
}

bool table_delete(table* table, obj_string* key)
{
    if (table->count == 0) return false;

    entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

obj_string* table_find_string(table* table, const char* chars, int length, uint32_t hash)
{
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;

    for (;;) {
        entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

/* SCANNER OPERATIONS */
scanner* init_scanner(char* source)
{
    scanner* s = (scanner*)malloc(sizeof(scanner));
    s->start = source;
    s->current = source;
    s->line = 1;
    return s;
}

static char peek_next(scanner* scanner)
{
    if (*scanner->current == '\0')
        return '\0';
    return scanner->current[1];
}

static token make_token(scanner* scanner, token_type type)
{
    token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;

    return token;
}

static token_type check_keyword(scanner* scanner, int start, int length, const char* rest, token_type type)
{
    if (scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
        return type;

    return TOKEN_IDENTIFIER;
}

static token_type identifier_type(scanner* scanner)
{
    switch (scanner->start[0]) {
        case 'a': return check_keyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(scanner, 1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1){
                switch (scanner->start[1]) {
                    case 'a': return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return check_keyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
        }
    return TOKEN_IDENTIFIER;
}

static token identifier(scanner* scanner)
{
    while (is_alpha(*scanner->current ) || is_digit(*scanner->current )) scanner->current++;

    return make_token(scanner, identifier_type(scanner));
}

static token scan_number(scanner* scanner)
{
    while (is_digit(*scanner->current)) scanner->current++;;

    if (*scanner->current == '.' && is_digit(peek_next(scanner))) {
        scanner->current++;
        while(is_digit(*scanner->current)) scanner->current++;
    }

    return make_token(scanner, TOKEN_NUMBER);
}

static token error_token(scanner* scanner, char* message)
{
    token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;

    return token;
}

static bool scan_match(scanner* scanner, char expected)
{
    if (*scanner->current == '\0') return false;
    if (*scanner->current != expected) return false;

    *scanner->current++;
    return true;
}

static void skip_whitespace(scanner* scanner)
{
    for(;;) { 
        char c = *scanner->current;
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                scanner->current++;
                break;
            case '\n':
                scanner->line++;
                scanner->current++;
                break;
            case '/':
                if (peek_next(scanner) == '/') {
                    while (*scanner->current != '\n' && !(*scanner->current == '\0'))
                        scanner->current++;
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

static token scan_string(scanner* scanner)
{
    while (*scanner->current != '"' && !(*scanner->current == '\0')) {
        if (*scanner->current == '\n') scanner->line++;
        scanner->current++;
    }

    if (*scanner->current == '\0') return error_token(scanner, "Unterminated string");

    scanner->current++;
    return make_token(scanner, TOKEN_STRING);
}

token scan_token(scanner* scanner)
{
    skip_whitespace(scanner);
    scanner->start = scanner->current;

    if(*scanner->current == '\0') 
        return make_token(scanner, TOKEN_EOF);

    scanner->current++;
    char c = scanner->current[-1];

    if(is_alpha(c)) return identifier(scanner);
    if (is_digit(c)) return scan_number(scanner);

    switch (c) {
    case '(': return make_token(scanner, TOKEN_LEFT_PAREN);
    case ')': return make_token(scanner, TOKEN_RIGHT_PAREN);
    case '{': return make_token(scanner, TOKEN_LEFT_BRACE);
    case '}': return make_token(scanner, TOKEN_RIGHT_BRACE);
    case ';': return make_token(scanner, TOKEN_SEMICOLON);
    case ',': return make_token(scanner, TOKEN_COMMA);
    case '.': return make_token(scanner, TOKEN_DOT);
    case '-': return make_token(scanner, TOKEN_MINUS);
    case '+': return make_token(scanner, TOKEN_PLUS);
    case '/': return make_token(scanner, TOKEN_SLASH);
    case '*': return make_token(scanner, TOKEN_STAR);
    case '!': return make_token(scanner, scan_match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return make_token(scanner, scan_match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return make_token(scanner, scan_match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return make_token(scanner, scan_match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return scan_string(scanner);
    }

    return error_token(scanner, "Unexpected character.");
}

/* DEBUGGER OPERATIONS */
static int jump_instruction(const char* name, int sign, chunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassemble_instruction(chunk* chunk, int offset)
{
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) printf("   | ");
    else printf("%4d ", chunk->lines[offset]);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            uint8_t constant = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_CONSTANT", constant, AS_NUMBER(chunk->constants.values[constant]));
            return offset + 2;
        case OP_NIL:
            printf("OP_NIL\n");
            return offset + 1;
        case OP_TRUE:
            printf("OP_TRUE\n");
            return offset + 1;
        case OP_FALSE:
            printf("OP_FALSE\n");
            return offset + 1;
        case OP_POP:
            printf("OP_POP");
            return offset + 1;
        case OP_GET_LOCAL:
            uint8_t local_get = chunk->code[offset + 1];
            printf("%-16s %4d\n", "OP_GET_LOCAL", local_get);
            return offset + 2;
        case OP_SET_LOCAL:
            uint8_t local_set = chunk->code[offset + 1];
            printf("%-16s %4d\n", "OP_SET_LOCAL", local_set);
            return offset + 2;
        case OP_GET_GLOBAL:
            uint8_t global_get = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_GET_GLOBAL", global_get, AS_NUMBER(chunk->constants.values[global_get]));
            return offset + 2;
        case OP_DEFINE_GLOBAL:
            uint8_t global_def = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_DEFINE_GLOBAL", global_def, AS_NUMBER(chunk->constants.values[global_def]));
            return offset + 2;
        case OP_SET_GLOBAL:
            uint8_t global_set = chunk->code[offset + 1];
            printf("%-16s %4d '%g'\n", "OP_SET_GLOBAL", global_set, AS_NUMBER(chunk->constants.values[global_set]));
            return offset + 2;
        case OP_EQUAL:
            printf("OP_EQUAL\n");
            return offset + 1;
        case OP_GREATER:
            printf("OP_GREATER\n");
            return offset + 1;
        case OP_LESS:
            printf("OP_LESS\n");
            return offset + 1;
        case OP_ADD:
            printf("OP_ADD\n");
            return offset + 1;
        case OP_SUBTRACT:
            printf("OP_SUBTRACT\n");
            return offset + 1;
        case OP_MULTIPLY:
            printf("OP_MULTIPLY\n");
            return offset + 1;
        case OP_DIVIDE:
            printf("OP_DIVIDE\n");
            return offset + 1;
        case OP_NOT:
            printf("OP_NOT");
            return offset + 1;
        case OP_NEGATE:
            printf("OP_NEGATE\n");
            return offset + 1;
        case OP_PRINT:
            printf("OP_PRINT\n");
            return offset + 1;
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_RETURN:
            printf("OP_RETURN\n");
            return offset + 1;
        default:
            printf("Unknown opcode (%d)\n", instruction);
    }

    return offset + 1;
}

void disassemble_chunk(chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

/* VIRTUAL MACHINE OPERATIONS */
static inline void push(VM* vm, value val) { *(vm->stack_top++) = val; }
static inline value pop(VM* vm) { return *(--vm->stack_top); }
static inline value peek(VM* vm, int distance) { return vm->stack_top[-1 - distance]; }
static inline bool is_falsey(value val) { return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val)); }

static interpret_result runtime_error(VM* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);

    return INTERPRET_RUNTIME_ERROR;
}

static bool values_equal(value a, value b)
{
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:
            return AS_STRING(a)->length == AS_STRING(b)->length &&
                    memcmp(AS_STRING(a)->chars, AS_STRING(b)->chars, AS_STRING(a)->length) == 0;
        default:
            return false;
    }
}

static interpret_result check(VM* vm)
{
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
        return runtime_error(vm, "Operands must be numbers");

    return INTERPRET_OK;
}

static void print_value(value val)
{
    switch (val.type) {
        case VAL_BOOL:
            printf(AS_BOOL(val) ? "true" : "false"); break;
        case VAL_NIL:
            printf("nil"); break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(val)); break;
        case VAL_OBJ: 
            switch (OBJ_TYPE(val)) {
                case OBJ_STRING:
                    printf("%s", AS_CSTRING(val));
                    break;
            }
            break;
    }
}

static void concatenate(VM* vm)
{
    obj_string* b = AS_STRING(pop(vm));
    obj_string* a = AS_STRING(pop(vm));

    int length = a->length + b->length;
    char* chars = (char*)malloc(sizeof(char) * (length + 1));
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    uint32_t hash = hash_string(chars, length);
    obj_string* result = allocate_string(vm, chars, length, hash);
    push(vm, OBJ_VAL(result));
}

static interpret_result run(VM* vm)
{
    double a, b;
    uint8_t instruction;

    while (1) {
        switch (instruction = (*vm->ip++)) {
            case OP_CONSTANT:
                value constant = vm->chunk->constants.values[(*vm->ip++)];
                push(vm, constant);
                break;
            case OP_NIL:
                push(vm, NIL_VAL);
                break;
            case OP_TRUE:
                push(vm, BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(vm, BOOL_VAL(false));
                break;
            case OP_POP:
                pop(vm);
                break;
            case OP_GET_LOCAL:
                push(vm, vm->stack[(*vm->ip++)]);
                break;
            case OP_SET_LOCAL:
                vm->stack[(*vm->ip++)] = peek(vm, 0);
                break;
            case OP_GET_GLOBAL:
                obj_string* global_name = AS_STRING(vm->chunk->constants.values[(*vm->ip++)]);
                value val;
                if (!table_get(&vm->globals, global_name, &val))
                    return runtime_error(vm, "Undefined variable '%s'", global_name->chars);
                push(vm, val);
                break;
            case OP_DEFINE_GLOBAL:
                obj_string* global_def = AS_STRING(vm->chunk->constants.values[(*vm->ip++)]);
                table_set(&vm->globals, global_def, peek(vm, 0));
                pop(vm);
                break;
            case OP_SET_GLOBAL:
                obj_string* global_set = AS_STRING(vm->chunk->constants.values[(*vm->ip++)]);
                if (table_set(&vm->globals, global_set, peek(vm, 0))) {
                    table_delete(&vm->globals, global_set);
                    return runtime_error(vm, "Undefined variable '%s'", global_set->chars);
                }
                break;
            case OP_EQUAL:
                push(vm, BOOL_VAL(values_equal(pop(vm), pop(vm))));
                break;
            case OP_GREATER:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;
                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));

                push(vm, NUMBER_VAL(a > b));
                break;
            case OP_LESS:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                push(vm, NUMBER_VAL(a < b));
                break;
            case OP_ADD:
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1)))
                    concatenate(vm);
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1)))
                    push(vm, NUMBER_VAL(AS_NUMBER(pop(vm)) + AS_NUMBER(pop(vm))));
                else
                    return runtime_error(vm, "Operands must be two numbers or two strings\n");
                break;
            case OP_SUBTRACT:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a - b));
                break;
            case OP_MULTIPLY:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                push(vm, NUMBER_VAL(AS_NUMBER(pop(vm)) * AS_NUMBER(pop(vm))));
                break;
            case OP_DIVIDE:
                if (check(vm))
                    return INTERPRET_RUNTIME_ERROR;

                b = AS_NUMBER(pop(vm)); a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a / b));
                break;
            case OP_NOT:
                push(vm, BOOL_VAL(is_falsey(pop(vm))));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0)))
                    return runtime_error(vm, "Operand must be a number");

                push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            case OP_PRINT: 
                print_value(pop(vm));
                printf("\n");
                break;
            case OP_JUMP:
                vm->ip += (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]));
                break;
            case OP_JUMP_IF_FALSE:
                if (is_falsey(peek(vm, 0)))
                    vm->ip += (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]));
                break;
            case OP_LOOP:
                vm->ip -= (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]));
                break;
            case OP_RETURN:
                /* Exit interpreter */
                return INTERPRET_OK;
        }
    }
}

VM* init_vm()
{
    VM* vm = (VM*)malloc(sizeof(VM));

    vm->stack_top = vm->stack;
    vm->objects = NULL;
    vm->strings.count = 0;
    vm->strings.capacity = 0;
    vm->strings.entries = NULL;
    vm->globals.count = 0;
    vm->globals.capacity = 0;
    vm->globals.entries = NULL;

    return vm;
}

void free_vm(VM* vm)
{
    /* Free allocated strings */
	struct Obj* object = vm->objects;
	while (object != NULL) {
		struct Obj* next = object->next;
		switch (object->type) {
			case OBJ_STRING:
				obj_string* str = (obj_string*)object;
				free(str->chars);
				free(str);
		}
		object = next;
	}

	/* Free virtual machine */
	free(vm->strings.entries);
	free(vm->globals.entries);
	free(vm);
}

interpret_result interpret(polity_interpreter* interpreter, char* source)
{
    interpreter->chunk = (chunk*)calloc(1, sizeof(chunk));

    if (!compile(source, interpreter)) {
        free_chunk(interpreter->chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    interpreter->vm->chunk = interpreter->chunk;
    interpreter->vm->ip = interpreter->chunk->code;

    interpret_result result = run(interpreter->vm);

    free_chunk(interpreter->chunk);
    return result;
}