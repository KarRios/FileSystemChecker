CC=gcc
CFLAGS= -Wall -g

all:
	$(CC) -o xcheck $(CFLAGS) xcheck.c

clean:
	$(RM) xcheck
