CC=gcc
CLIENT=client
SERVER=server

ifeq ($(DEBUG),)
  # by default debug is on
  DEBUG=y
endif

all: $(SERVER) $(CLIENT)

CFLAGS=-Wall -Werror
LDLIBS=-lpthread

ifeq ($(DEBUG),y)
  CFLAGS+=-g -DDEBUG
endif

# server
SERVER_OBJS=daemon.o server.o chat_io.o

$(SERVER): $(SERVER_OBJS)
	$(CC) -o $@ $(LDLIBS) $^

# client
CLIENT_OBJS=client.o chat_io.o

$(CLIENT): $(CLIENT_OBJS)
	$(CC) -o $@ $^

# rules
%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

clean:
	rm -f *.o

cleanall: clean
	rm -f *.o tags $(SERVER) $(CLIENT)

