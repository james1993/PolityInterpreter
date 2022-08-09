IDIR = ./include
CC=gcc
CFLAGS=-I$(IDIR) -g -O0

ODIR=src

_DEPS = common.h interpreter.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o interpreter.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

polity: $(OBJ)
	$(CC) -g -O0 -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~