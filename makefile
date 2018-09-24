server: chatserver.c
	gcc -o server.o chatserver.c

server.o: chatserver.c
	gcc -o server.o chatserver.c

clean: 
	rm -rf server.o

run: server.o
	./server.o