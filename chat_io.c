#include <stdlib.h>
#include "chat_io.h"

#ifdef DEBUG
char *tlv_str[] = {
    "CONNECT",
    "CONNECT_FAIL",
    "DISCONNECT",
    "LOGIN",
    "LOGIN_SUCCESS",
    "LOGIN_FAIL",
    "LOGIN_LOGGEDIN",
    "LOGOUT",
    "REGISTER",
    "REGISTER_SUCCESS",
    "REGISTER_FAIL_RESERVED",
    "REGISTER_FAIL_REREGISTER",
    "REGISTER_FAIL_CAPACITY",
    "UNREGISTER",
    "UNREGISTER_SUCCESS",
    "UNREGISTER_FAIL",
    "REMOVE",
    "FRIEND_ADD",
    "FRIEND_ADD_SUCCESS",
    "FRIEND_ADD_FAIL",
    "FRIEND_REMOVE",
    "FRIENDS_END",
    "FRIEND_LOGIN",
    "FRIEND_LOGOUT",
    "FRIEND_UNREGISTER",
    "COLLEGUE_ADD",
    "COLLEGUE_REMOVE",
    "IM",
    "CHAT_REQ",
    "CHAT_ACC",
    "CHAT_REJ",
};
#endif

/* setting the values of a mst_t:
 *   msg: pointer to a msg_t object
 *   type: type of message
 *   length: length of value
 *   value: message value
 * NOTE: the resulting field msg.length is larger than length by MSG_HDR_LN */
void msg_set_data(msg_t *msg, tlv_t type, int length, void *value)
{
    memset(msg, 0, sizeof(msg_t));
    msg->type = type;
    msg->length =length;
    memcpy(msg->value, value, MIN(length, MSG_MAX_VAL_LN));
}

/* setting a msg_t to recursively incorporate zero or more msg_t bojects */
int msg_set_tlv(msg_t *msg, tlv_t type, ...)
{
    va_list ap;
    msg_t *nexttlv;
    char *vptr = msg->value;
    int cnt = 0;

    msg->type = type;
    msg->length = MSG_HDR_LN;

    va_start(ap, type);
    while ((nexttlv = va_arg(ap, msg_t *)))
    {
	memcpy(vptr, (char *)nexttlv, nexttlv->length);
	vptr += nexttlv->length;
	msg->length += nexttlv->length;
	cnt++;
    }
    va_end(ap);

    return cnt;
}

msg_t *msg_alloc(void)
{
    return (msg_t *)calloc(1, sizeof(msg_t));
}

void msg_free(msg_t *msg)
{
    free(msg);
}

/* recieving a msg_t via a socket */
ssize_t msg_recv(msg_t *msg, sck_t sck)
{
    ssize_t ret = 0, length;
    tlv_t type;
    char msgbuf[MSG_MAX_VAL_LN];

    memset(msgbuf, 0, MSG_MAX_VAL_LN);
    if (recv(sck, &type, sizeof(tlv_t), 0) != sizeof(tlv_t))
	return -1;
    ret += sizeof(tlv_t);

    if ((recv(sck, &length, sizeof(int), 0) != sizeof(int)) || (length < 0))
	return -1;
    ret += sizeof(int);

    if (length && (recv(sck, msgbuf, length, 0) != length))
	return -1;
    ret += length;

    msg_set_data(msg, type, length, msgbuf);
    return ret;
}

/* sending a msg_t via a socket */
ssize_t msg_send(msg_t *msg, sck_t sck)
{
    return send(sck, msg, sizeof(tlv_t) + sizeof(int) + msg->length, 0);
}
