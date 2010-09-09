BIN       = statustran

SOURCES  += main.c
OBJ      += $(subst .c,.o,$(SOURCES))
DEPFILES  = $(subst .c,.d,$(SOURCES))

CC        = gcc

LIBS      = -ltrivfs -lfshelp -g
CFLAGS    = -Wall -Wextra -std=c89 -pedantic -g

.PHONY: clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o "$(BIN)" $(LIBS)
	rm -rf *.d

-include $(DEPFILES)

%.o : %.c
	$(CC) -c $< $(CFLAGS) -o $@

clean:
	rm -f *.o

%.d: %.c
	@echo "Generating dependency makefile for $<."
	@$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
