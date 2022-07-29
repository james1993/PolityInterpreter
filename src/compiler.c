#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source)
{
    scanner* s = init_scanner(source);
    int line = -1;
    for (;;) {  /* TODO: Infinite loops make me sad :( */
        token t = scan_token(s);
        if (t.line != line) {
            printf("%4d ", t.line);
            line = t.line;
        } else
            printf("   | ");

        printf("%2d '%.*s'\n", t.type, t.length, t.start);

        if (t.type == TOKEN_EOF) break;
    }
    free(s);
}