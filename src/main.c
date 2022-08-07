#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void run_file(VM* vm, const char* path)
{
	/* Read file */
	if (!path || strlen(path) < 3 || strcmp(&path[(int)strlen(path) - 3],".np")) {
		fprintf(stderr, "Must be file of type .np\n");
		exit(74);
	}

	FILE* file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	char* buffer = (char*)malloc(file_size + 1);
	fread(buffer, sizeof(char), file_size, file);
	buffer[file_size] = '\0';

	fclose(file);

	/* Execute polity source file */
	interpret_result result = interpret(vm, buffer);
	free(buffer);

	if (result == INTERPRET_COMPILE_ERROR) {
		printf("Compile error\n");
		exit(65);
	}
	if (result == INTERPRET_RUNTIME_ERROR) {
		printf("Runtime error\n");
		exit(70);
	}
}

int main(int argc, const char* argv[])
{
	VM* vm = init_vm();
	
	if (argc == 2) {
		run_file(vm, argv[1]);
	} else {
		fprintf(stderr, "Usage: polity [path_to_file.np]\n");
		exit(64);
	}

	free_vm(vm);
	return 0;
}