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

