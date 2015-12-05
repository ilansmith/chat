#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <sys/socket.h> 
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include "chat.h"

typedef enum {
    CONNECT,
    CONNECT_SUCCESS,
    CONNECT_FAIL,
    DISCONNECT,
    DISCONNECT_SUCCESS,
    DISCONNECT_FAIL,
    LOGIN,
    LOGIN_SUCCESS,
    LOGIN_FAIL,
    LOGIN_LOGGEDIN,
    LOGOUT, /* 10 */
    REGISTER,
    REGISTER_SUCCESS,
    REGISTER_FAIL,
    UNREGISTER,
    UNREGISTER_SUCCESS,
    UNREGISTER_FAIL,
    SEARCH,
    SEARCH_SUCCESS,
    SEARCH_FAIL,
    CLIENT_ADD_REQ, /* 20 */
    SERVER_ADD_REQ,
    CLIENT_ADD_ACCEPT,
    CLIENT_ADD_REJECT,
    SERVER_ADD_ACCEPT,
    SERVER_ADD_REJECT,
    SERVER_ADD_FAIL,
    REMOVE,
    FRIEND_ADD, /* used */
    FRIEND_ADD_SUCCESS, /* used */
    FRIEND_ADD_FAIL, /* 30 used */
    FRIEND_REMOVE, /* used */
    FRIENDS_END, /* used */
    FRIEND_LOGIN,
    FRIEND_LOGOUT,
    FRIEND_UNREGISTER, /* 35 */
    COLLEGUE_ADD,
    COLLEGUE_REMOVE,
    COLLEGUE_UNREGISTER,
    IM,
    CHAT_REQ,
    CHAT_ACC, /* 40 */
    PEER_HELLO,
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

