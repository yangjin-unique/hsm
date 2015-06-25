CC = gcc
CFLAGS = -g -Wall -Werror -o2
CLIB = -lpthread

hsm.o: hsm.c  hsm.h hsm_priv.h misc.h
	$(CC) $(CFLAGS) $< -o $@

misc.o: misc.c misc.h list.h heap-inl.h
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	-rm -f *.o
