#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <sys/socket.h> 
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include "chat.h"

typedef enum {
    CONNECT,
    CONNECT_FAIL,
    DISCONNECT,
    LOGIN,
    LOGIN_SUCCESS,
    LOGIN_FAIL,
    LOGIN_LOGGEDIN,
    LOGOUT,
    REGISTER,
    REGISTER_SUCCESS,
    REGISTER_FAIL_RESERVED,
    REGISTER_FAIL_REREGISTER,
    REGISTER_FAIL_CAPACITY,
    UNREGISTER,
    UNREGISTER_SUCCESS,
    UNREGISTER_FAIL,
    REMOVE,
    FRIEND_ADD,
    FRIEND_ADD_SUCCESS,
    FRIEND_ADD_FAIL,
    FRIEND_REMOVE,
    FRIENDS_END,
    FRIEND_LOGIN,
    FRIEND_LOGOUT,
    FRIEND_UNREGISTER,
    COLLEGUE_ADD,
    COLLEGUE_REMOVE,
    IM,
    CHAT_REQ,
    CHAT_ACC,
    CHAT_REJ,
} tlv_t;

#define MSG_HDR_LN (sizeof(tlv_t) + sizeof(int))
#define MSG_MAX_VAL_LN 1024
#define MSG_TYP(MSG) ((MSG)->type)
#define MSG_LEN(MSG) ((MSG)->length)
#define MSG_VAL(MSG) ((MSG)->value)
#define MSG_VAL_LN(MSG) ((MSG).lengt - MSG_HDR_LN)

#define MSG_VAL_FRIEND_ID(MSG) (*((long *)(MSG_VAL(MSG))))
#define MSG_VAL_FRIEND_ST(MSG) (*((int *)(MSG_VAL(MSG) + sizeof(long))))
#define MSG_VAL_FRIEND_NM(MSG) ((MSG_VAL(MSG) + sizeof(long) + sizeof(int)))

#define MSG_VAL_COLLEGUE_ID(MSG) (*((long *)(MSG_VAL(MSG))))
#define MSG_VAL_COLLEGUE_NM(MSG) ((char *)(MSG_VAL(MSG) + sizeof(long)))

/* colours */
#define COL_RESET "\033[00;00m"
/* #define COL_BLACK "\033[00;30m" */
#define COL_BRIGHT_BLACK "\033[01;30m"
/* #define COL_RED "\033[00;31m" */
#define COL_BRIGHT_RED "\033[01;31m"
/* #define COL_GREEN "\033[00;32m" */
#define COL_BRIGHT_GREEN "\033[01;32m"
/* #define COL_ORANGE "\033[00;33m" */
#define COL_YELLOW "\033[01;33m"
/* #define COL_BLUE "\033[00;34m" */
#define COL_BRIGHT_BLUE "\033[01;34m"
/* #define COL_MAGENTA "\033[00;35m" */
#define COL_BRIGHT_MAGENTA "\033[01;35m"
/* #define COL_CYAN "\033[00;36m" */
#define COL_BRIGHT_CYAN "\033[01;36m"
/* #define COL_GREY "\033[00;37m" */
 #define COL_BRIGHT_GREY "\033[01;37m"
/* #define COL_WHITE "\033[00;38m" */
#define COL_BRIGHT_WHITE "\033[01;38m"
#define COL_BG_BLACK "\033[00;40m"
#define COL_BG_BRIGHT_RED "\033[01;41m"
/* #define COL_BG_BRIGHT_GREY "\033[01;47m" */

/* colour definitions */
#define COLOUR_DEBUG COL_BRIGHT_BLACK 
#define COLOUR_APP_NAME COL_BRIGHT_CYAN 
#define COLOUR_ERROR_PROMPT COL_BG_BRIGHT_RED
#define COLOUR_ERROR COL_YELLOW
#define COLOUR_STATUS_PROMPT COL_BRIGHT_CYAN
#define COLOUR_STATUS COL_BRIGHT_WHITE
#define COLOUR_AVAILABLE_CMDS COL_BRIGHT_CYAN
#define COLOUR_HELP_CMD COL_BRIGHT_WHITE
#define COLOUR_HELP COL_BRIGHT_GREY
#define COLOUR_CLIENT_PROMPT COL_BRIGHT_CYAN 
#define COLOUR_CLIENT COL_BRIGHT_BLUE 
/* #define COLOUR_DAEMON COL_BRIGHT_WHITE */
#define COLOUR_FEEDBACK COL_BRIGHT_GREEN
#define COLOUR_PEER_PROMPT COL_BRIGHT_RED 
#define COLOUR_PEER COL_BRIGHT_MAGENTA 
#define COLOUR_CLEAR COL_BRIGHT_WHITE

#define COLOUR_RESET                                                          \
    fprintf(stdout, "%s", COL_RESET);                                         \
    fflush(stdout);                                                           \
    fprintf(stderr, "%s", COL_RESET);                                         \
    fflush(stderr);

#define SET_COLOUR(STREAM, COLOUR)                                            \
    fprintf(STREAM, "%s%s", COL_BG_BLACK, COLOUR);                            \
    fflush(STREAM);

#define PRINT_FEEDBACK(FMT, ...)                                              \
    SET_COLOUR(stdout, COLOUR_FEEDBACK);                                      \
    fprintf(stdout, FMT "\n", ##__VA_ARGS__);                                 \
    SET_COLOUR(stdout, COLOUR_CLEAR);

#define PRINT_ERROR(FMT, ...)                                                 \
    SET_COLOUR(stderr, COLOUR_CLEAR);                                         \
    SET_COLOUR(stderr, COL_BG_BRIGHT_RED);                                    \
    fprintf(stderr, "error:");                                                \
    SET_COLOUR(stderr, COLOUR_CLEAR);                                         \
    SET_COLOUR(stderr, COL_YELLOW);                                           \
    fprintf(stderr, " " FMT "\n", ##__VA_ARGS__);                             \
    SET_COLOUR(stderr, COL_RESET);

#define PRINT_INTRO(FMT, ...)                                                 \
    SET_COLOUR(stdout, COLOUR_CLEAR);                                         \
    fprintf(stdout, FMT "\n", ##__VA_ARGS__);                                 \
    SET_COLOUR(stdout, COLOUR_CLEAR);

#define PRINT_DEBUG(FMT, ...);                                                \
    SET_COLOUR(stdout, COLOUR_DEBUG);                                         \
    fprintf(stdout, FMT "\n", ##__VA_ARGS__);                                 \
    SET_COLOUR(stdout, COL_RESET);

typedef struct msg_t {
    tlv_t type;
    int length;
    char value[MSG_MAX_VAL_LN];
} msg_t;

/* externs */
extern char *tlv_str[];

/* prototypes */
void msg_set_data(msg_t *msg, tlv_t type, int length, void *value);
int msg_set_tlv(msg_t *msg, tlv_t type, ...);
msg_t *msg_alloc(void);
void msg_free(msg_t *msg);
ssize_t msg_recv(msg_t *msg, sck_t sck);
ssize_t msg_send(msg_t *msg, sck_t sck);

#endif

