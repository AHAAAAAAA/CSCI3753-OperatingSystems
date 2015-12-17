# File: Makefile
# Project: CSCI 3753 Programming Assignment 5
# Description:
#	This is the Makefile for PA5.

CC           = gcc

CFLAGSFUSE   = `pkg-config fuse --cflags`
LLIBSFUSE    = `pkg-config fuse --libs`
LLIBSOPENSSL = -lcrypto

CFLAGS = -c -g -Wall -Wextra
LFLAGS = -g -Wall -Wextra

XATTR_EXAMPLES = xattr-util
OPENSSL_EXAMPLES = aes-crypt-util 

.PHONY: all xattr-examples openssl-examples clean

all: xattr-examples openssl-examples pa5-encfs

xattr-examples: $(XATTR_EXAMPLES)
openssl-examples: $(OPENSSL_EXAMPLES)

pa5-encfs: pa5-encfs.o aes-crypt.o
	$(CC) $(LFLAGS) $^ -o $@ $(LLIBSFUSE) $(LLIBSOPENSSL)

xattr-util: xattr-util.o
	$(CC) $(LFLAGS) $^ -o $@

aes-crypt-util: aes-crypt-util.o aes-crypt.o
	$(CC) $(LFLAGS) $^ -o $@ $(LLIBSOPENSSL)

pa5-encfs.o: pa5-encfs.c
	$(CC) $(CFLAGS) $(CFLAGSFUSE) $<

xattr-util.o: xattr-util.c
	$(CC) $(CFLAGS) $<

aes-crypt-util.o: aes-crypt-util.c aes-crypt.h
	$(CC) $(CFLAGS) $<

aes-crypt.o: aes-crypt.c aes-crypt.h
	$(CC) $(CFLAGS) $<

clean:
	rm -f $(XATTR_EXAMPLES)
	rm -f $(OPENSSL_EXAMPLES)
	rm -f *.o
	rm -f *~
	rm -f pa5-encfs