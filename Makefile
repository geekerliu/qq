all:server client
server:server.o common.h common.o
	gcc $^ -o $@ -lpthread
client:client.o common.h common.o
	gcc $^ -o $@ -lpthread
common.o:common.c
	gcc $^ -c
clean:
	rm -rf *.o server client
