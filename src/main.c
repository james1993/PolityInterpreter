#include <stdlib.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
	VM* vm = init_vm();
	chunk* ch = (chunk*)calloc(1,sizeof(chunk));

	int constant = add_constant(ch, 1.2);
	write_chunk(ch, OP_CONSTANT, 123);
	write_chunk(ch, constant, 123);

	constant = add_constant(ch, 3.4);
 	write_chunk(ch, OP_CONSTANT, 123);
 	write_chunk(ch, constant, 123);
 
 	write_chunk(ch, OP_ADD, 123);
 
 	constant = add_constant(ch, 5.6);
 	write_chunk(ch, OP_CONSTANT, 123);
 	write_chunk(ch, constant, 123);
 
 	write_chunk(ch, OP_DIVIDE, 123);
	write_chunk(ch, OP_NEGATE, 123);
	write_chunk(ch, OP_RETURN, 123);

	interpret(vm, ch);

	free(vm);
	free_chunk(ch); 
}