#include <stdlib.h>
#include "chat_io.h"

#ifdef DEBUG
char *tlv_str[] = {
    "CONNECT",
    "CONNECT_SUCCESS",
    "CONNECT_FAIL",
    "DISCONNECT",
    "DISCONNECT_SUCCESS",
    "DISCONNECT_FAIL",
    "LOGIN",
    "LOGIN_SUCCESS",
    "LOGIN_FAIL",
    "LOGIN_LOGGEDIN",
    "LOGOUT",
    "REGISTER",
    "REGISTER_SUCCESS",
    "REGISTER_FAIL",
    "UNREGISTER",
    "UNREGISTER_SUCCESS",
    "UNREGISTER_FAIL",
    "SEARCH",
    "SEARCH_SUCCESS",
    "SEARCH_FAIL",
    "CLIENT_ADD_REQ",
    "SERVER_ADD_REQ",
    "CLIENT_ADD_ACCEPT",
    "CLIENT_ADD_REJECT",
    "SERVER_ADD_ACCEPT",
    "SERVER_ADD_REJECT",
    "SERVER_ADD_FAIL",
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
    "COLLEGUE_UNREGISTER",
    "IM",
    "CHAT_REQ",
    "CHAT_ACC",
    "PEER_HELLO",
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

msg_t *msg_alloc()
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
    if (((ret += recv(sck, &type, sizeof(tlv_t), 0)) < 0) ||
	((ret += recv(sck, &length, sizeof(int), 0)) < 0) ||
	(length && (ret += recv(sck, msgbuf, length, 0)) < 0))
    {
	return (ssize_t)ret;
    }

    msg_set_data(msg, type, length, msgbuf);
    return ret;
}

/* sending a msg_t via a socket */
ssize_t msg_send(msg_t *msg, sck_t sck)
{
    return send(sck, msg, sizeof(tlv_t) + sizeof(int) + msg->length, 0);
}
