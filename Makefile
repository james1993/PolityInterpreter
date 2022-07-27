IDIR = ./include
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=src

_DEPS = common.h memory.h debug.h chunk.h value.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o memory.o debug.o chunk.o value.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

polity_interpreter: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~