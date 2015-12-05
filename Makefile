all: server client 

server: daemon.o server.o chat_io.o
	gcc -g -Wall -o $@ daemon.o server.o chat_io.o -lpthread 

daemon.o: chat.h server.h daemon.c
	gcc -g -Wall -o $@ -c daemon.c

server.o: chat.h server.h server.c
	gcc -g -Wall -o $@ -c server.c

client: client.o chat_io.o
	gcc -g -Wall -o $@ client.o chat_io.o

client.o: chat.h client.h client.c
	gcc -g -Wall -o $@ -c client.c

chat_io.o: chat.h chat_io.h chat_io.c
	gcc -g -Wall -o $@ -c chat_io.c
	
clean:
	rm -f *.o

cleanall:
	rm -f *.o tags server client

