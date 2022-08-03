#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl(VM* vm)
{
	char line[1024];
	for(;;) {
		printf("> ");

		if(!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}

		interpret(vm, line);
	}
}

static char* read_file(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	char* buffer = (char*)malloc(file_size + 1);
	if (!buffer) {
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}
	size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
	if (bytes_read < file_size) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(74);
	}
	buffer[bytes_read] = '\0';

	fclose(file);
	return buffer;
}

static void run_file(VM* vm, const char* path)
{
	char* source = read_file(path);
	interpret_result result = interpret(vm, source);
	free(source);

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
	
	if (argc == 1) {
		repl(vm);
	} else if (argc == 2) {
		run_file(vm, argv[1]);
	} else {
		fprintf(stderr, "Usage: polity [path]\n");
		exit(64);
	}

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

	if(vm->strings.entries) {
		free(vm->strings.entries);
	}

	free(vm);

	return 0;
}