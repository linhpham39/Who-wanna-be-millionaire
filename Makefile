CC = g++
CFLAGS = -std=c++11 -Wall

all: client server

client: client.o
	$(CC) $(CFLAGS) -o client client.o

client.o: client.cpp
	$(CC) $(CFLAGS) -c client.cpp

server: server.o
	$(CC) $(CFLAGS) -o server server.o -lpthread

server.o: server.cpp
	$(CC) $(CFLAGS) -c server.cpp

clean:
	rm -f client server *.o
