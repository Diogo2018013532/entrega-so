CC     = gcc
CFLAGS = -Wall -Wextra -g -pthread

all: central

central: central.c comum.h
	$(CC) $(CFLAGS) -o central central.c

clean:
	rm -f central

.PHONY: all clean