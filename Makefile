IDIR = ./include
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=src

_DEPS = common.h interpreter.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o interpreter.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

polity: $(OBJ)
	$(CC) -g -O1 -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~