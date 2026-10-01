CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -D_DEFAULT_SOURCE

all: client server

client: client.c common.c common.h msg_struct.h
	$(CC) $(CFLAGS) client.c common.c -o client

server: server.c client_list.c client_list.h common.c common.h msg_struct.h
	$(CC) $(CFLAGS) server.c client_list.c common.c -o server

clean:
	rm -f client server

.PHONY: all clean
