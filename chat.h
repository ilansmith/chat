#ifndef _CHAT_H_
#define _CHAT_H_

#define DEBUG

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

#define STATUS_CONNECTED 1
#define STATUS_DISCONNECTED 0

#define INPUT_BUFFER_SZ 1024
#define INPUT_PARAM_SZ 256

#define CONN_PROTO_INIT "tcp"
#define CONN_PROTO_HNDL "tcp"
#define CONN_PORT 5555

#define ASCII_COPYRIGHT 0xa9
#define PRINT(arg, ...) printf(arg "\n", ##__VA_ARGS__)

#define STR_NIL ""
#define STR_MAX_COMMAND_LENGTH 11
#define STR_MAX_NAME_LENGTH 9
#define STR_MAX_HOST_NAME_LENGTH 255
#define STR_MAX_IM_LENGTH 251
#define PARAM_LIST_SZ 2 /* may be redundent */

#define ERRC_CLIENT 0x1
#define ERRC_DAEMON 0x2
#define ERRC_SERVER 0x3

#define ERRT_NONE 0x1
#define ERRT_ALLOC 0x2
#define ERRT_REALLOC 0x3
#define ERRT_SOCKET 0x4
#define ERRT_BIND 0x5
#define ERRT_LISTEN 0x6
#define ERRT_ACCEPT 0x7
#define ERRT_CONNECT 0x8
#define ERRT_RECV 0x9
#define ERRT_SEND 0xa
#define ERRT_GETPROTOBYNAME 0xb
#define ERRT_READ_INPUT 0xc
#define ERRT_INET_ADDR 0xd
#define ERRT_SIGNIN 0xe
#define ERRT_LOGIN 0xf
#define ERRT_POSTMSG 0x10
#define ERRT_EXTRACTMSG 0x11
#define ERRT_PEERINSERT 0x12

#define ERRA_PANIC 0x1
#define ERRA_WARN 0x2

#ifdef _DAEMON_
#define PANIC_EXIT exit(1);
#endif

#ifdef _SERVER_
#define PANIC_EXIT s_exit(svr, 1);
#endif

#ifdef _CLIENT_
#define PANIC_EXIT c_panic();
#endif

#define ERROR_HANDLE(ACTION, ...)                                             \
        printf("\nerror: "),                                                  \
        ##__VA_ARGS__;                                                        \
	switch (ACTION)                                                       \
        {                                                                     \
        case ERRA_PANIC:                                                      \
	    PANIC_EXIT                                                        \
	    break;                                                            \
	case ERRA_WARN:                                                       \
            break;                                                            \
	}

#define FAIL_EQUAL_ZERO(VALUE, ACTION, ...)                                   \
	if (!(VALUE))                                                         \
	{                                                                     \
	    ERROR_HANDLE(ACTION, ##__VA_ARGS__);                              \
	}

#define FAIL_LESS_THAN_ZERO(VALUE, ACTION, ...)                               \
	if ((VALUE) < 0)                                                      \
	{                                                                     \
	    ERROR_HANDLE(ACTION, ##__VA_ARGS__);                              \
	}

#define FAIL_NOT_EQUAL_ONE(VALUE, ACTION, ...)                                \
	if ((VALUE) != 1)                                                     \
	{                                                                     \
	    ERROR_HANDLE(ACTION, ##__VA_ARGS__);                              \
	}

#define __ASSERT(VALUE, ERROR, ACTION, ...)                                     \
	switch (ERROR)                                                        \
	{                                                                     \
	case ERRT_ALLOC:                                                      \
	case ERRT_REALLOC:                                                    \
	case ERRT_GETPROTOBYNAME:                                             \
	case ERRT_INET_ADDR:                                                  \
	case ERRT_EXTRACTMSG:                                                 \
	    FAIL_EQUAL_ZERO(VALUE, ACTION, ##__VA_ARGS__);                    \
	    break;                                                            \
	case ERRT_SOCKET:                                                     \
	case ERRT_BIND:                                                       \
	case ERRT_LISTEN:                                                     \
	case ERRT_ACCEPT:                                                     \
	case ERRT_CONNECT:                                                    \
	case ERRT_RECV:                                                       \
	case ERRT_SEND:                                                       \
	case ERRT_READ_INPUT:                                                 \
	case ERRT_LOGIN:                                                      \
	case ERRT_POSTMSG:                                                    \
	case ERRT_PEERINSERT: 						      \
	    FAIL_LESS_THAN_ZERO(VALUE, ACTION, ##__VA_ARGS__);                \
	    break;                                                            \
	case ERRT_SIGNIN:                                                     \
	    FAIL_NOT_EQUAL_ONE(VALUE, ACTION, ##__VA_ARGS__);                 \
	    break;                                                            \
	}


#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) <= (Y) ? (Y) : (X))

/* TODO: find better arrangement */
typedef struct cfriend_t {
    long fid;
    int fconnected;
    char fname[STR_MAX_NAME_LENGTH];
    struct cfriend_t *next;
} cfriend_t;

typedef int sck_t;
typedef struct in_addr in_addr;
typedef struct protoent protoent;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;

#endif

