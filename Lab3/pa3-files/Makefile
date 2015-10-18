CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -pthread -Wall -Wextra

.PHONY: all clean

all: multi-lookup

multi-lookup: multi-lookup.o queue.o util.o
	$(CC) $(LFLAGS) $^ -o $@

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) $<

util.o: util.c util.h
	$(CC) $(CFLAGS) $<

multi-lookup.o: multi-lookup.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f multi-lookup
	rm -f *.o
	rm -f *~