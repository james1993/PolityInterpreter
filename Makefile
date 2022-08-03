IDIR = ./include
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=src

_DEPS = common.h debug.h chunk.h vm.h compiler.h scanner.h table.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o debug.o chunk.o vm.o compiler.o scanner.o table.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

polity_interpreter: $(OBJ)
	$(CC) -g -O1 -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~