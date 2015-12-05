#ifndef _SERVER_H_
#define _SERVER_H_

#include "chat.h"
#include "chat_io.h"

#define MEMBERS_INIT_SZ 10
#define MSG_QUEUE_LENGTH 10

#define DUMMY_USER &dummy_user
#define DUMMY_CID -1
#define DUMMY_MUTEX 2
#define DUMMY_CONNECTION 0

#define LOCK(MTX) (MTX) = 0
#define UNLOCK(MTX) (MTX) = 1
#define LOCKED(MTX) !(MTX)
#define CS_START(MTX)                                \
	while (LOCKED(MTX))                          \
	    select(0, NULL, NULL, NULL, &zero_tm);   \
	if ((MTX) != DUMMY_MUTEX)                    \
	    LOCK(MTX)
#define CS_END(MTX) UNLOCK(MTX)
#define SVR_CID(S) ((S)->cid)
#define SVR_SCK(S) ((S)->sck)
#define SVR_STATUS(S) ((S)->status)

typedef enum {
    SSTAT_INIT,
    SSTAT_LOGIN,
    SSTAT_RELOGIN,
    SSTAT_REGISTER_SUCCESS,
    SSTAT_REGISTER_FAIL,
    SSTAT_SERVE,
    SSTAT_CHAT_REQUEST,
    SSTAT_DISCONNECT,
    SSTAT_LOGOUT,
    SSTAT_UNREGISTER,
    SSTAT_ERROR,
} sstat_t;

typedef int mtx_t;

typedef struct svr_t {
    int cid;
    sck_t sck;
    sstat_t status;
} svr_t;

typedef struct peer_t {
    long id;
    struct peer_t *next;
} sfriend_t, scollegue_t;

typedef struct mail_t {
    msg_t *msg;
    struct mail_t *next;
} mail_t;

typedef struct user_t {
    long cid;                      /* client's id (index in users array)      */
    mtx_t mutex;
    char name[STR_MAX_NAME_LENGTH];/* client's screen name                    */
    int connected;                 /* STATUS_CONNECTED or STATUS_DISCONNECTED */
    sfriend_t *friends;            /* a list of friends                       */
    scollegue_t *collegues;        /* a list of collegues                     */
    mail_t *mailbox;               /* a mail box for receiving messages       */
} user_t;

extern user_t **users;
extern user_t dummy_user;
extern long usr_count;
extern long usr_array_size;
extern struct timeval zero_tm;
extern mtx_t mtx_usrcnt;

/* prototypes */
int usr_array_enlarge(void);
void *s_handle(void *sck);

#endif

