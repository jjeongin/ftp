CC = gcc
CFLAGS = -Wall -g

# ****************************************************
all: server_dir/server client_dir/client

server: server_dir/server.c
	$(CC) $(CFLAGS) server_dir/server.c -o server_dir/server

client: client_dir/client.c
	$(CC) $(CFLAGS) client_dir/client.c -o client_dir/client

clean:
	rm -f