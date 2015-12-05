#define _SERVER_

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "server.h"

#define _PSVR(STREAM, NAME, FMT, ...) \
    fprintf(STREAM, "server");                       \
    if (strcmp(STR_NIL, NAME))                       \
        fprintf(STREAM, " (%s)", NAME);              \
    fprintf(STREAM, ": " FMT "%s\n",  ##__VA_ARGS__, \
	COL_RESET);     \
    fflush(stderr);

#define PSVR(NAME, FMT, ...) _PSVR(stdout, NAME, FMT, ##__VA_ARGS__);
#define PSVRERR(NAME, FMT, ...) _PSVR(stderr, NAME, FMT, ##__VA_ARGS__);

#define ASSERT(VALUE, ERROR, ACTION, NAME, FMT, ...) __ASSERT(VALUE, ERROR,   \
	ACTION, PSVRERR(NAME, FMT, ##__VA_ARGS__))

/* free a mail_t and the msg_t it wraps */
static void mail_free(mail_t *mail)
{
    if (mail->msg)
	msg_free(mail->msg);

    free(mail);
}


/* allocate a new mail_t */

static mail_t *mail_alloc(void)
{
    mail_t *mail = NULL;
    msg_t *msg = NULL;

    if (!(mail = (mail_t *)calloc(1, sizeof(mail_t))))
	return NULL;

    if (!(msg = msg_alloc()))
    {
	mail_free(mail);
	return NULL;
    }

    mail->msg = msg;
    return mail;
}

/* free a mail_t link and the msg_t it wraps from within a mail_t link list
 * return:
 *  0 on success
 * -1 on failure */
static int mail_delete(mail_t **mailptr)
{
    mail_t *temp = NULL;

    if (!mailptr || !(*mailptr))
	return -1;

    temp = *mailptr;
    *mailptr = (*mailptr)->next;
    mail_free(temp);

    return 0;
}

static void mail_set_data(mail_t *mail, tlv_t type, int length, char *value)
{
    msg_set_data(mail->msg, type, length, value);
}

static sfriend_t *sfriend_alloc(long id)
{
    sfriend_t *friend = NULL;

    if (!(friend = (sfriend_t *)calloc(1, sizeof(sfriend_t))))
	return NULL;

    friend->id = id;
    return friend;
}

static void sfriend_free(sfriend_t *friend)
{
    free(friend);
}

static scollegue_t *scollegue_alloc(long id)
{
    scollegue_t *collegue = NULL;
    
    if (!(collegue = (scollegue_t *)calloc(1, sizeof(scollegue_t))))
	return NULL;

    collegue->id = id;
    return collegue;
}

static void scollegue_free(scollegue_t *collegue)
{
    free(collegue);
}
static void s_exit(svr_t *svr, int status)
{
    if (!svr) /* sanity check */
	goto Exit;

    if (SVR_CID(svr) != -1)
    {
	CS_START(users[SVR_CID(svr)]->mutex);
	/* no need to verify dummy user is connected */
	users[SVR_CID(svr)]->connected = 0;
	CS_END(users[SVR_CID(svr)]->mutex);
#ifdef DEBUG
	PSVR(users[SVR_CID(svr)]->name, "user '%s' logged out",
	    users[SVR_CID(svr)]->name);
#endif
    }

    close(SVR_SCK(svr));
    free(svr);

Exit:
#ifdef DEBUG
    PSVR(STR_NIL, "disconnected");
#endif
    pthread_exit(&status);
}

static user_t *user_alloc(const char *name, long cid)
{
    user_t *user = (user_t *)calloc(1, sizeof(user_t));

    if (user)
    {
	user->cid = cid;
	memcpy(user->name, name, STR_MAX_NAME_LENGTH);
	UNLOCK(user->mutex);
    }

    return user;
}

static void user_free(user_t *user)
{
    sfriend_t *friend = user->friends, *tmpf = NULL;
    scollegue_t *collegue = user->collegues, *tmpc =NULL;
    mail_t *mailbox = user->mailbox, *tmpm = NULL;

    while ((tmpf = friend))
    {
	friend = friend->next;
	sfriend_free(tmpf);
    }

    while ((tmpc = collegue))
    {
	collegue = collegue->next;
	scollegue_free(tmpc);
    }

    while ((tmpm = mailbox))
    {
	mailbox = mailbox->next;
	mail_free(tmpm);
    }

    free(user);
}

/* post mail with contents type from sender to receiver
 * this function assumes that it is being called inside a critical section
 * protected by receiver->mutex
 * return:
 *  0 on success
 * -1 on failure*/
static int s_mail_post(user_t *sender, user_t *receiver, tlv_t type)
{
    mail_t *new_mail = NULL, **last_mail = NULL;
    char buffer[INPUT_BUFFER_SZ];

    /* critical section protected by the calling function */

    /* check for message sending rights and creation of new_mail */
    if ((!(receiver->connected) && (type != FRIEND_UNREGISTER) &&
	(type != COLLEGUE_ADD) && (type != COLLEGUE_REMOVE)))

    {
#ifdef DEBUG
	PSVR(sender->name, "could not post a %s message to '%s' who is offline",
	    tlv_str[type], receiver->name);
#endif
	return 0;
    }

    if (!(new_mail = mail_alloc()))
	return -1;

    memset(buffer, 0, INPUT_BUFFER_SZ);
    /* all mail messages contain sender id at the begining of the data */
    memcpy(buffer, &(sender->cid), sizeof(long));

    /* creating message for receiver's mailbox */
    switch (type)
    {
    /* TODO: chat request/accept/reject messages */
    /* in the following cases it is not necessary to send the name         */
    case FRIEND_LOGIN:     /* friends must exist to send these messages    */
    case FRIEND_LOGOUT:
    case COLLEGUE_REMOVE:  /* user does not get notified                   */
	mail_set_data(new_mail, type, sizeof(long), buffer);
	break;
    /* in this case the name must be sent for use after the unregistration */
    case FRIEND_UNREGISTER:
    case COLLEGUE_ADD:
	memcpy(buffer + sizeof(long), sender->name, MIN(strlen(sender->name),
	    STR_MAX_NAME_LENGTH));
	mail_set_data(new_mail, type, sizeof(long) + MIN(strlen(sender->name), 
	    STR_MAX_NAME_LENGTH), buffer);
	break;
    default:
#ifdef DEBUG
	PSVR(sender->name, "unknown type of message to be posted to '%s'", 
	    receiver->name);
#endif
	mail_free(new_mail);
	return -1;
    }

    /* adding mail to the end of receiver->id's mailbox 
     * if a message already exists in the mailbox with the same sender id and 
     * type, new_mail will be disregarded */
    last_mail = &(receiver->mailbox);
    while (*last_mail)
    {
	/* a sender cannot send the same message more than once before it gets
	 * handeld by the receiver */
	if ((MSG_TYP((*last_mail)->msg) == type) &&
	    (*((long *)MSG_VAL((*last_mail)->msg)) == sender->cid))
	{
	    mail_free(new_mail);
#ifdef DEBUG
	    PSVR(sender->name, "user '%s' already has a pending %s message - "
		"did not resend", receiver->name, tlv_str[type]);
#endif
	    return 0;
	}

	/* if there is a COLLEGUE_ADD followed by a COLLEGUE_REMOVE in the 
	 * receiver's mailbox, both from the same sender, remove them */
	if ((type == COLLEGUE_REMOVE) && 
	    (*((long *)MSG_VAL((*last_mail)->msg)) == sender->cid) &&
	    (MSG_TYP((*last_mail)->msg) == COLLEGUE_ADD))
	{
	    mail_free(new_mail);
	    new_mail = *last_mail;
	    *last_mail = (*last_mail)->next;
	    mail_free(new_mail);
#ifdef DEBUG
	    PSVR(sender->name, "removed user '%s's pending COLLEGUE_ADD "
		"message", receiver->name);
#endif
	    return 0;
	}
	
	last_mail = &((*last_mail)->next);
    }
    *last_mail = new_mail;

#ifdef DEBUG
    PSVR(sender->name, "posted a %s message to user '%s'", tlv_str[type],
	receiver->name);
#endif

    /* end of critical section */
    return 0;
}

/* insert a friend entry into the the user's list of friends
 * return:
 * 0 on success
 * 1 on failure */
static int s_sfriend_insert(svr_t *svr, long id)
{
    sfriend_t *friend = NULL;

    if (!(friend = sfriend_alloc(id)))
	return -1;

    friend->next = users[SVR_CID(svr)]->friends;
    users[SVR_CID(svr)]->friends = friend;
    return 0;
}

/* extract a friend entry from the the user's list of friends */
static void s_sfriend_extract(svr_t *svr, long id)
{
    sfriend_t **friend = &(users[SVR_CID(svr)]->friends), *temp = NULL;

    while (*friend && ((*friend)->id != id))
	friend = &((*friend)->next);

    if (!(*friend))
	return;

    temp = *friend;
    *friend=(*friend)->next;

    sfriend_free(temp);
}

/* send the user friend details */
static int s_sfriend_add(svr_t *svr, long friend_id)
{
    char array[INPUT_BUFFER_SZ];
    char *aptr = array;
    int name_length, success;
    msg_t out_msg;

    memset(array, 0, INPUT_BUFFER_SZ);

    CS_START(users[friend_id]->mutex);
    if (users[friend_id] == DUMMY_USER)
	return -1;

    /* setting friend's cid */
    memcpy(aptr, (&(users[friend_id]->cid)), sizeof(long));
    aptr += sizeof(long);

    /* setting friend's connection status */
    memcpy(aptr, (&(users[friend_id]->connected)), sizeof(int));
    aptr += sizeof(int);

    /* setting friend's name */
    memcpy(aptr, users[friend_id]->name, name_length =
	MIN(strlen(users[friend_id]->name), MIN(STR_MAX_NAME_LENGTH,
	INPUT_BUFFER_SZ - (sizeof(long) + sizeof(int)))));
    CS_END(users[friend_id]->mutex);

    msg_set_data(&out_msg, FRIEND_ADD, sizeof(long) + sizeof(int) + 
	name_length, array);

    success = msg_send(&out_msg, SVR_SCK(svr));

#ifdef DEBUG
    PSVR(users[SVR_CID(svr)]->name, "sent user friend details for '%s'", 
	users[friend_id]->name);
#endif

    return success;
}

/* insert a collegue entry into the list of collegues */
static int s_scollegue_insert(user_t *user, long col_id)
{
    scollegue_t *collegue = NULL;
    
    if (!(collegue = scollegue_alloc(col_id)))
	return -1;

    collegue->next = user->collegues;
    user->collegues = collegue;

#ifdef DEBUG
	PSVR(user->name, "added '%s' to list of collegues", 
	    users[col_id]->name);
#endif

    return 0;
}

static void s_scollegue_extract(user_t *user, long col_id)
{
    scollegue_t **collegue = &(user->collegues), *temp = NULL;

    while (*collegue)
    {
	if ((*collegue)->id != col_id)
	{
	    collegue = &((user->collegues)->next);
	    continue;
	}

	temp = *collegue;
	*collegue = (*collegue)->next;
	scollegue_free(temp);

#ifdef DEBUG
	PSVR(user->name, "removed '%s' from list of collegues", 
	    users[col_id]->name);
#endif

	break;
    }
}

/* travers a mail_t link list and handle the messages in accordance to
 * SVR_STATUS(svr)
 * when SVR_STATUS(svr) == SSTAT_SERVE or SVR_STATUS(SVR) == SSTAT_LOGOUT, this 
 * function assumes that it is being called inside a critical section protected
 * by users[SVR_CID(svr)]->mutex */
static void s_sweep_mailbox(svr_t *svr, user_t *user)
{
/* validate the the message gets handled in the current SVR_STATUS(svr) */
#define VALIDATE_STATE(PRED) if (!(PRED))  \
        {                                  \
	    mailptr = &((*mailptr)->next); \
	    break;                         \
	}

    mail_t **mailptr = &(user->mailbox);
    msg_t *msg = NULL;
    long i;

    /* critical section protected by the calling function */
    while (*mailptr)
    {
	if (!(msg = (*mailptr)->msg))
	{
	    mail_delete(mailptr);
	    continue;
	}
	
	switch (MSG_TYP((*mailptr)->msg))
	{
	/* done at: logon, logoff, unregister */
	case COLLEGUE_REMOVE:
	    VALIDATE_STATE(/*(SVR_STATUS(svr) == SSTAT_SERVE) ||*/
		(SVR_STATUS(svr) == SSTAT_LOGIN) ||
		(SVR_STATUS(svr) == SSTAT_LOGOUT) ||
		(SVR_STATUS(svr) == SSTAT_UNREGISTER));

	    s_scollegue_extract(user, *((long *)(msg->value)));
	    mail_delete(mailptr);
	    break;
	/* done at: unregister */
	case COLLEGUE_ADD:
	    VALIDATE_STATE(/*(SVR_STATUS(svr) == SSTAT_SERVE) || */
		(SVR_STATUS(svr) == SSTAT_UNREGISTER));

	    ASSERT(s_scollegue_insert(user, *((long *)(msg->value))),
		ERRT_PEERINSERT, ERRA_WARN, user->name, "could not add '%s' to "
		"list of collegues", users[i]->name);
	    mail_delete(mailptr);
	    break;
	/* done at: logon */
	case FRIEND_UNREGISTER:
	    VALIDATE_STATE(/*(SVR_STATUS(svr) == SSTAT_SERVE) ||*/
		(SVR_STATUS(svr) == SSTAT_LOGIN));

	    s_sfriend_extract(svr, *((long *)MSG_VAL(msg)));
	    ASSERT(msg_send(msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN, 
		user->name, "could not send user message about the logout of "
		"'%s'", (char *)MSG_VAL(msg));
	    mail_delete(mailptr);
	    break;
	default:
	    mail_delete(mailptr);
	    break;
	}
    }
    /* end of critical section */
}

/* travers list of collegues and send them a message: either FRIEND_LOGIN,
 * FRIEND_LOGOUT or FRIEND_UNREGISTER */
static void s_sweep_collegues(svr_t *svr, user_t *user, tlv_t type)
{
    scollegue_t *collegue = user->collegues;

    while (collegue)
    {
	CS_START(users[collegue->id]->mutex);
	ASSERT(s_mail_post(user, users[collegue->id], type), ERRT_POSTMSG, 
	    ERRA_WARN, user->name, "could not allocate a mail message to send "
	    "to '%s'", users[collegue->id]->name);
	CS_END(users[collegue->id]->mutex);

	collegue = collegue->next;
    }
}

/* travers list of friends and initiate them at user.
 * done during login */
static void s_sweep_friends(svr_t *svr, user_t *user)
{
    sfriend_t *friend = user->friends;
    msg_t out_msg;
    int success;

    switch (SVR_STATUS(svr))
    {
    case SSTAT_LOGIN:
	while (friend)
	{
	    success = s_sfriend_add(svr, friend->id);
	    friend = friend->next;
	}

	msg_set_data(&out_msg, FRIENDS_END, 0, NULL);
	msg_send(&out_msg, SVR_SCK(svr));
	break;
    case SSTAT_UNREGISTER:
	while (friend)
	{
	    CS_START(users[friend->id]->mutex);
	    if (users[friend->id] == DUMMY_USER)
		continue;
	    ASSERT(success = s_mail_post(user, users[friend->id], 
		COLLEGUE_REMOVE), ERRT_POSTMSG, ERRA_WARN, user->name, 
		"could not send user '%s' an unregister notification", 
		users[friend->id]->name);
	    CS_END(users[friend->id]->mutex);
	    friend = friend->next;
	}
    default:
	/* TODO: sanity check */
	break;
    }
}

static void s_command_send_im(svr_t *svr, msg_t *msg)
{
    mail_t *new_mail = NULL, **last_mail = NULL;
    char buffer[INPUT_BUFFER_SZ];
    int len;
    long fid = *((long *)MSG_VAL(msg));
    
    memset(buffer, 0, INPUT_BUFFER_SZ);
    strncpy(buffer, users[SVR_CID(svr)]->name, STR_MAX_NAME_LENGTH);
    strncpy(buffer + strlen(users[SVR_CID(svr)]->name) + 1, MSG_VAL(msg) + 
	sizeof(long), STR_MAX_IM_LENGTH);
    len = strlen(buffer) + 1;
    len += strlen(buffer + len) +1;

    CS_START(users[fid]->mutex);
    if (users[fid] == DUMMY_USER)
	return;
    ASSERT((int)(new_mail = mail_alloc()), ERRT_ALLOC, ERRA_WARN, 
	users[SVR_CID(svr)]->name, "could not allocte an instant message to "
	"send to '%s'", users[fid]->name);

    if (!new_mail)
    {
	CS_END(users[fid]->mutex);
	return;
    }

    /* insert sender's id in instant message */
    memcpy(MSG_VAL(msg), &SVR_CID(svr), sizeof(long));

    mail_set_data(new_mail, IM, len, buffer);
    last_mail = &(users[fid]->mailbox);
    while (*last_mail)
	last_mail = &((*last_mail)->next);
    *last_mail = new_mail;

#ifdef DEBUG
    PSVR(users[SVR_CID(svr)]->name, "posted instatnt message to '%s'",
	users[fid]->name);
#endif
    CS_END(users[fid]->mutex);
}

/* handle a request from the user to add name to its list of friends */
static void s_command_add(svr_t *svr, char *name)
{
    msg_t out_msg;
    int success = -1;
    long i;
    ssize_t sz;

    for (i = 0; i < usr_array_size; i++)
    {
	CS_START(users[i]->mutex);
	/* no user[i] */
	if (users[i] == DUMMY_USER)
	    continue;

	/* user[i] is not the requested user */
	if (strncmp(users[i]->name, name, STR_MAX_NAME_LENGTH))		
	{
	    CS_END(users[i]->mutex);
	    continue;
	}

	/* user[i] is the requested user */

	/* send users[i] a COLLEGUE_ADD mail  */
	ASSERT(success = s_mail_post(users[SVR_CID(svr)], users[i], 
	    COLLEGUE_ADD), ERRT_POSTMSG, ERRA_WARN, users[SVR_CID(svr)]->name, 
	    "could not send user '%s' an add collegue request", name);
	CS_END(users[i]->mutex);
	break;
    }

    /* a user with the name 'name' was not found */
    if (success == -1)
	goto Error;

    /* add a friend to the list of friends */
    ASSERT(success = s_sfriend_insert(svr, i), ERRT_PEERINSERT, ERRA_WARN,
	users[SVR_CID(svr)]->name, "could not allocate entry for new friend "
	"'%s'", name);

    /* could not allocate a sfriend_t for the new friend */
    if (success == -1)
	goto Error;

    /* send friend initialization to user */
    msg_set_data(&out_msg, FRIEND_ADD_SUCCESS, sizeof(long), &i);
    ASSERT(sz = msg_send(&out_msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN,
        users[SVR_CID(svr)]->name, "could not send friend initialization for "
        "%s", name);

    if (sz == -1)
    {
	/* TODO handle failure of sending message to user */
    }

    return;

Error:
    msg_set_data(&out_msg, FRIEND_ADD_FAIL, 0, NULL);
    ASSERT(msg_send(&out_msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN,
        users[SVR_CID(svr)]->name, "could not add '%s' as a friend", name);
}

static void s_command_remove(svr_t *svr, long id)
{
    s_sfriend_extract(svr, id);

    CS_START(users[id]->mutex);
    if (users[id] == DUMMY_USER)
	return;
    /* send users[id] a COLLEGUE_REMOVE mail  */
    ASSERT(s_mail_post(users[SVR_CID(svr)], users[id], COLLEGUE_REMOVE),
	ERRT_POSTMSG, ERRA_WARN, users[SVR_CID(svr)]->name, "could not send "
	"user '%s' a remove collegue request", users[id]->name);
    CS_END(users[id]->mutex);
}

/* logging out
 * scan mailbox for removed collegues
 * scan collegues and send logout notifications */
static void s_command_logout(svr_t *svr)
{
#ifdef DEBUG
	PSVR(users[SVR_CID(svr)]->name, "received logout request");
#endif
	CS_START(users[SVR_CID(svr)]->mutex);
	s_sweep_mailbox(svr, users[SVR_CID(svr)]);
	users[SVR_CID(svr)]->connected = 0;
	CS_END(users[SVR_CID(svr)]->mutex);

	s_sweep_collegues(svr, users[SVR_CID(svr)], FRIEND_LOGOUT);

#ifdef DEBUG
	PSVR(users[SVR_CID(svr)]->name, "user '%s' logged out",
	    users[SVR_CID(svr)]->name);
#endif
	SVR_CID(svr) = -1;
	SVR_STATUS(svr) = SSTAT_INIT;
}

static void s_handle_command(svr_t *svr)
{
    msg_t in_msg;

    ASSERT(msg_recv(&in_msg, SVR_SCK(svr)), ERRT_RECV, ERRA_WARN,
	users[SVR_CID(svr)]->name, "could not receive command");

#ifdef DEBUG
    PSVR(users[SVR_CID(svr)]->name, "received a %s command from user", 
	tlv_str[MSG_TYP(&in_msg)]);
#endif

    switch(MSG_TYP(&in_msg))
    {
    case IM:
	s_command_send_im(svr, &in_msg);
	break;
    case CHAT_REQ:
	/* TODO (change server's status) */
	break;
    case CHAT_ACC:
	/* TODO */
	break;
    case CHAT_REJ:
	/* TODO */
	break;
    case FRIEND_ADD:
	s_command_add(svr, MSG_VAL(&in_msg));
	break;
    case FRIEND_REMOVE:
	s_command_remove(svr, *((long *)MSG_VAL(&in_msg)));
	break;
    case LOGOUT:
	SVR_STATUS(svr) = SSTAT_LOGOUT;
	s_command_logout(svr);
	break;
    case DISCONNECT:
	s_command_logout(svr);
	SVR_STATUS(svr) = SSTAT_DISCONNECT;
	break;
    default:
	SVR_STATUS(svr) = SSTAT_ERROR;
	break;
    }
}

static int s_mail_receive_im(svr_t *svr, msg_t *msg)
{
    int success;

#ifdef DEBUG
    PSVR(users[SVR_CID(svr)]->name, "received a IM message from '%s'",
	MSG_VAL(msg));
#endif

    if (!msg)
	return -1;

    ASSERT(success = msg_send(msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN, 
	users[SVR_CID(svr)]->name, "could not send an instant message");

#ifdef DEBUG
    if (success != -1)
	PSVR(users[SVR_CID(svr)]->name, "sent user an IM message");
#endif

    return (success <= 0) ? -1 : 0;
}

static int s_mail_friend_status(svr_t *svr, msg_t *msg)
{
    int success;

    if (!msg)
	return -1;

    ASSERT(success = msg_send(msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN,
        users[SVR_CID(svr)]->name, "could not send a friend %s notification "
	"message to users", (*((tlv_t *)MSG_TYP(msg)) == FRIEND_LOGIN) ?
	"login" : "logout");

#ifdef DEBUG
    if (success != -1)
	PSVR(users[SVR_CID(svr)]->name, "sent user a %s notification",
	tlv_str[MSG_TYP(msg)]);
#endif

    return (success <= 0) ? -1 : 0;
}

/* receive notification that friend has unregisterred */
static int s_mail_friend_unregister(svr_t *svr, msg_t *msg)
{
    int success;

    s_sfriend_extract(svr, *((long *)MSG_VAL(msg)));
    ASSERT(success = msg_send(msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN, 
	users[SVR_CID(svr)]->name, "could not sent a friend unregister "
	"notification");

#ifdef DEBUG
    PSVR(users[SVR_CID(svr)]->name, "sent a %s message to user",
	tlv_str[MSG_TYP(msg)]);
#endif

    return success;
}

/* handle a mail request to add a collegue 
 * gurenteed that msg != NULL */
static int s_mail_collegue_add(svr_t *svr, msg_t *msg)
{
    int success;
    long collegue_id;
    char *collegue_name = NULL;

    if (!msg)
	return -1;
    collegue_id = *((long *)MSG_VAL(msg));
    collegue_name = MSG_VAL(msg) + sizeof(long);

    /* adding collegue to list of collegues */
    s_scollegue_insert(users[SVR_CID(svr)], collegue_id);

    /* notifying user */
    ASSERT(success = msg_send(msg, SVR_SCK(svr)), ERRT_SEND, ERRA_WARN,
	users[SVR_CID(svr)]->name, "could not notify user about new collegue "
	"'%s'", collegue_name);

#ifdef DEBUG
    if (success != -1)
    {
	PSVR(users[SVR_CID(svr)]->name, "notifed user of new collegue '%s'", 
	    collegue_name);
    }
#endif

    /* notifying collegue of user's status (online only!) */
    CS_START(users[collegue_id]->mutex);
    ASSERT(success = s_mail_post(users[SVR_CID(svr)], users[collegue_id], 
	FRIEND_LOGIN), ERRT_POSTMSG, ERRA_WARN, users[SVR_CID(svr)]->name, 
	"could not notify collegue '%s' of user's status", collegue_name);
    CS_END(users[collegue_id]->mutex);

    /* TODO: test success */
    return 0;
}

static void s_handle_mail(svr_t *svr)
{
    mail_t *mail = NULL, **mailbox = &(users[SVR_CID(svr)]->mailbox);

    /* extract the first mail in the mailbox */
    CS_START(users[SVR_CID(svr)]->mutex);
    if (!mailbox || !(*mailbox) || !((*mailbox)->msg))
    {
	CS_END(users[SVR_CID(svr)]->mutex);
	SVR_STATUS(svr) = SSTAT_ERROR;
	return;
    }

    mail = *mailbox;
    *mailbox = (*mailbox)->next;
    CS_END(users[SVR_CID(svr)]->mutex);

#ifdef DEBUG
    if (MSG_TYP(mail->msg) != IM)
    {
	PSVR(users[SVR_CID(svr)]->name, "received a %s message from '%s'",
	    tlv_str[MSG_TYP(mail->msg)],
	    users[*((long *)MSG_VAL(mail->msg))]->name);
    }
#endif

    switch (MSG_TYP(mail->msg))
    {
    case IM:
	s_mail_receive_im(svr, mail->msg);
	break;
    case CHAT_REQ:
	/* TODO */
	break;
    case CHAT_ACC:
	/* TODO */
	break;
    case CHAT_REJ:
	/* TODO */
	break;
    case FRIEND_LOGIN:
	s_mail_friend_status(svr, mail->msg);
	break;
    case FRIEND_LOGOUT:
	s_mail_friend_status(svr, mail->msg);
	break;
    case FRIEND_UNREGISTER:
	s_mail_friend_unregister(svr, mail->msg);
	break;
    case COLLEGUE_ADD:
	s_mail_collegue_add(svr, mail->msg);
	break;
    case COLLEGUE_REMOVE:
	s_scollegue_extract(users[SVR_CID(svr)], *((long *)MSG_VAL(mail->msg)));
	break;
    default:
	/* TODO: sanity check */
	break;
    }

    /* free the first mail in the mailbox */
    mail_free(mail);
}

/* entering status: SSTAT_SERVE */
static void s_serve(svr_t *svr)
{
#define SLEEP select(0, NULL, NULL, NULL, &tv)
#define TV_SEC 0
#define TV_USEC 10

    struct timeval tv;
    fd_set rfds;
    int request, serve = 1;

    CS_START(users[SVR_CID(svr)]->mutex);
    /* update list of collegues and report unregisterred friends */
    s_sweep_mailbox(svr, users[SVR_CID(svr)]);
    /* set user as connected */
    users[SVR_CID(svr)]->connected = 1;
    CS_END(users[SVR_CID(svr)]->mutex);
    /* send collegues a LOGIN message */
    s_sweep_collegues(svr, users[SVR_CID(svr)], FRIEND_LOGIN);
    /* initiate friends at user */
    s_sweep_friends(svr, users[SVR_CID(svr)]);

    /* serve */
    SVR_STATUS(svr) = SSTAT_SERVE;
#ifdef DEBUG
    PSVR(users[SVR_CID(svr)]->name, "ready to chat");
#endif
    while (serve)
    {
	switch (SVR_STATUS(svr))
	{
	case SSTAT_SERVE:
	    /* wait for user commands or peer messages */
	    do
	    {
		tv.tv_sec = TV_SEC;
		tv.tv_usec = TV_USEC;
		FD_ZERO(&rfds);
		FD_SET(SVR_SCK(svr), &rfds);
	    }
	    while (!(request = select(SVR_SCK(svr) + 1, &rfds, NULL, NULL,
		&tv)) && !(users[SVR_CID(svr)]->mailbox));

	    if (request == -1)
	    {
		SVR_STATUS(svr) = SSTAT_ERROR;
		break;
	    }

	    /* check for user command */
	    if (FD_ISSET(SVR_SCK(svr), &rfds))
	    {
		s_handle_command(svr);
		if ((SVR_STATUS(svr) != SSTAT_SERVE))
		    break;
	    }

	    /* check for peer messages */
	    if (users[SVR_CID(svr)]->mailbox)
		s_handle_mail(svr);
	    break;

	case SSTAT_CHAT_REQUEST:
	    /* TODO */
	    break;

	case SSTAT_DISCONNECT:
	    /* fall through */
	default:
	    serve = 0;
	    break;
	}
    }
}

/* login in a registered client
 * on return the server's status can be:
 *  SSTAT_INIT, as is the entry status
 *  SSTAT_RELOGIN, in the case that the user is already logged in
 *  SSTAT_SERVE, in the case of successful login */
static void s_login(svr_t *svr, char *cname)
{
    long i;

    for (i = 0; i < usr_array_size; i++)
    {
	CS_START(users[i]->mutex);
	if (users[i] == DUMMY_USER)
	    continue;

	if (!memcmp(users[i]->name, cname, MAX(strlen(users[i]->name),
	    strlen(cname))))
	{
	    if (users[i]->connected)
	    {
		SVR_STATUS(svr) = SSTAT_RELOGIN;
		CS_END(users[i]->mutex);
		return;
	    }
	    SVR_STATUS(svr) = SSTAT_LOGIN;
	    users[i]->connected = 1;
	    SVR_CID(svr) = i;
	    CS_END(users[i]->mutex);
	    break;
	}
	CS_END(users[i]->mutex);
    }
}

/* verify that cname is not reserved */
static int s_is_reserved(char *cname)
{
    char **ptr = NULL;
    char *reserved[] =
    {
	"none",
	"online",
	"offline",
	NULL,
    };

    for (ptr = reserved; *ptr; ptr++)
    {
	if (!strcasecmp(cname, *ptr))
	    return 1;
    }

    return 0;
}

/* register a new client */
static tlv_t s_register(char *cname)
{
    long i, first_free_index;

    /* verify that cname is not reserved */
    if (s_is_reserved(cname))
    {
#ifdef DEBUG
	PSVR(STR_NIL, "register faild, '%s' is reserved", cname);
#endif

	return REGISTER_FAIL_RESERVED;
    }

    CS_START(mtx_usrcnt);
    first_free_index = -1;

    for (i = 0; i < usr_array_size; i++)
    {
	/* hold first free place in users array */
	if (users[i] == DUMMY_USER)
	{
	    if (first_free_index == -1)
		first_free_index = i;
	    continue;
	}

	/* verify that a user with the same name is not already registered */
	if (!memcmp(users[i]->name, cname, STR_MAX_NAME_LENGTH))
	{
	    first_free_index = -1;
	    CS_END(mtx_usrcnt);
#ifdef DEBUG
	    PSVR(STR_NIL, "register faild, user '%s' is already registered",
		cname);
#endif

	    return REGISTER_FAIL_REREGISTER;
	}
    }

    /* users[] is full */
    if (first_free_index == -1)
	return REGISTER_FAIL_CAPACITY;

    /* allocate new user */
    if (!(users[first_free_index] = user_alloc(cname, first_free_index)))
    {
	CS_END(mtx_usrcnt);
#ifdef DEBUG
	PSVR (STR_NIL, "memory allocation failure, user %s could not be "
	    "registered", cname);
#endif

	return REGISTER_FAIL_CAPACITY;
    }

    usr_count++;
    CS_END(mtx_usrcnt);
#ifdef DEBUG
    PSVR(STR_NIL, "user '%s' registered successfuly", cname);
#endif

    return REGISTER_SUCCESS;
}

static int s_unregister(svr_t *svr, char *cname)
{
    long i;
    user_t *temp = NULL;

    for (i = 0; i < usr_array_size; i++)
    {
	/* users[i] is not the user to be unregistered */
	CS_START(users[i]->mutex);
	if (users[i] == DUMMY_USER || memcmp(users[i]->name, cname,
	    MIN(strlen(users[i]->name), strlen(cname))))
	{
	    CS_END(users[i]->mutex);
	    continue;
	}

	/* user[i] is the user to unregister but is logged in */
	if (users[i]->connected)
	{
#ifdef DEBUG
	    PSVR(STR_NIL, "unregister faild, user '%s' is logged in", cname);
#endif
	    CS_END(users[i]->mutex);
	    return  -1;
	}

	/* user[i] is the user to unregister and is not logged in */
	CS_START(mtx_usrcnt);
	temp = users[i];
	users[i] = DUMMY_USER; /* unlocks users[i]->mutex */
	usr_count--;
	CS_END(mtx_usrcnt);

	SVR_STATUS(svr) = SSTAT_UNREGISTER;
	s_sweep_mailbox(svr, temp);
	s_sweep_friends(svr, temp);
	s_sweep_collegues(svr, temp, FRIEND_UNREGISTER);

	user_free(temp);
#ifdef DEBUG
	PSVR(STR_NIL, "user '%s' has been unregistered", cname);
#endif

	SVR_CID(svr) = -1;
	SVR_STATUS(svr) = SSTAT_INIT;
	return 0;
    }

#ifdef DEBUG
    PSVR(STR_NIL, "unregister faild, user '%s' is not registered", cname);
#endif
    return -1;
}

/* initiate identity of user
 * possible commands from user:
 * register <user_name>
 * unregister <user_name>
 * login <user_name>
 * disconnect */
static void s_init(svr_t *svr)
{
    char cname[STR_MAX_NAME_LENGTH];
    msg_t in_msg, out_msg;
    tlv_t msg_type;

    SVR_CID(svr) = -1; /* initiating client id */
    while (SVR_STATUS(svr) == SSTAT_INIT)
    {
	/* receive login/register request from client */
	ASSERT(msg_recv(&in_msg, SVR_SCK(svr)), ERRT_RECV, ERRA_PANIC, 
	    STR_NIL, "could not receive data from users");

        switch (MSG_TYP(&in_msg))
	{
	case LOGIN:
	    memset(cname, 0, STR_MAX_NAME_LENGTH);
	    memcpy(cname, (char *)MSG_VAL(&in_msg), MIN(MSG_LEN(&in_msg),
		STR_MAX_NAME_LENGTH));

	    s_login(svr, cname);
	    switch (SVR_STATUS(svr))
	    {
	    case SSTAT_LOGIN:
		msg_type = LOGIN_SUCCESS;
#ifdef DEBUG
		PSVR(cname, "user '%s' logged in", cname);
#endif
		break;
	    case SSTAT_RELOGIN:
		msg_type = LOGIN_LOGGEDIN;
		SVR_STATUS(svr) = SSTAT_INIT;
#ifdef DEBUG
		PSVR(STR_NIL, "login failed, user '%s' is already logged in",
		    cname);
#endif
		break;
	    case SSTAT_INIT:
		msg_type = LOGIN_FAIL;
#ifdef DEBUG
		PSVR(STR_NIL, "login failed, user '%s' is not registered",
		    cname);
#endif
		break;
	    default:
		/*TODO: sanity check */
		break;
	    }
	    break;

	case REGISTER:
	    if (usr_count == MEMBERS_INIT_SZ)
	    {
		msg_type = REGISTER_FAIL_CAPACITY;
		break;
	    }
	    memset(cname, 0, STR_MAX_NAME_LENGTH);
	    memcpy(cname, (char *)MSG_VAL(&in_msg), MIN(MSG_LEN(&in_msg),
		STR_MAX_NAME_LENGTH));

	    /* TODO: bug - same message for users[] full and re-register */
	    msg_type = s_register(cname);
	    break;

	case UNREGISTER:
	    memset(cname, 0, STR_MAX_NAME_LENGTH);
	    memcpy(cname, (char *)MSG_VAL(&in_msg), MIN(MSG_LEN(&in_msg),
		STR_MAX_NAME_LENGTH));

	    msg_type = s_unregister(svr, cname) ? UNREGISTER_FAIL : 
		UNREGISTER_SUCCESS;
	    break;

	case DISCONNECT:
	    SVR_STATUS(svr) = SSTAT_DISCONNECT;
	    break;

	default:
#ifdef DEBUG
	    PSVR(STR_NIL, "undefined request of service");
#endif
	    msg_type = CONNECT_FAIL;
	    SVR_STATUS(svr) = SSTAT_ERROR;
	    break;
	}

	msg_set_data(&out_msg, msg_type, 0, NULL);
	msg_send(&out_msg, SVR_SCK(svr));
    }
}

/* handling server thread routine */
void *s_handle(void *server)
{
    svr_t *svr = (svr_t *)server;

#ifdef DEBUG
    PSVR(STR_NIL, "connected");
#endif
    SVR_STATUS(svr) = SSTAT_INIT;
    while (SVR_STATUS(svr) != SSTAT_DISCONNECT)
    {
	switch (SVR_STATUS(svr))
	{
	case SSTAT_INIT:
	    s_init(svr);
	    break;
	case SSTAT_LOGIN:
	    s_serve(svr);
	    break;
	case SSTAT_ERROR:
	    s_exit(svr, 1);
	    break;
	default:
	    SVR_STATUS(svr) = SSTAT_DISCONNECT;
	    break;
	}
    }

    s_exit(svr, 0);
    return NULL;
}

