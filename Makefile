# Simple Makefile for TCP calculator (C, POSIX)
CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11
LDFLAGS=

SRC_DIR=src
INC_DIR=include
BIN_SERVER=server
BIN_CLIENT=client

.PHONY: all clean server client

all: server client

server: $(SRC_DIR)/server.c $(INC_DIR)/proto.h
	$(CC) $(CFLAGS) -I$(INC_DIR) $(SRC_DIR)/server.c -o $(BIN_SERVER) $(LDFLAGS)

client: $(SRC_DIR)/client.c $(INC_DIR)/proto.h
	$(CC) $(CFLAGS) -I$(INC_DIR) $(SRC_DIR)/client.c -o $(BIN_CLIENT) $(LDFLAGS)

clean:
	rm -f $(BIN_SERVER) $(BIN_CLIENT)
