#define _MAIN_THREAD_

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "server.h"

#define PDMN(FMT, ...) printf("daemon: " FMT "\n", ##__VA_ARGS__);            \
                       fflush(stdout);
#define ASSERT(VALUE, ERROR, ACTION, FMT, ...) __ASSERT(VALUE, ERROR, ACTION, \
	PDMN(FMT, ##__VA_ARGS__))

user_t **users;
user_t dummy_user;
long usr_count;
long usr_array_size;
struct timeval zero_tm;
mtx_t mtx_usrcnt;

/* reallocating memory for more users */
/*
int usr_array_enlarge()
{
    int i;

    if (!realloc(users, 2 * usr_array_size))
	return -1;

    / * initializing the newly allocated memory * /
    for (i = usr_array_size; i < 2 * usr_array_size; i++)
	users[i] = 0;

    usr_array_size *= 2;
    return 0;
}
*/

static svr_t *alloc_svr()
{
    return (svr_t *)calloc(1, sizeof(svr_t));
}

/* daemon initiation function */
static sck_t d_init()
{
#define MEMBERS_INIT_SZ 10
#define QLEN 10

    int i;
    sck_t daemon;
    protoent *prtp = NULL;
    sockaddr_in svr_addr;

    /* initiation of mutexs and time object */
    zero_tm.tv_sec = 0;
    zero_tm.tv_usec = 0;
    UNLOCK(mtx_usrcnt);

    /* initiation of dummy user */
    dummy_user.cid = DUMMY_CID;
    dummy_user.mutex = DUMMY_MUTEX;
    memset(dummy_user.name, 0, STR_MAX_NAME_LENGTH);
    dummy_user.connected = DUMMY_CONNECTION;
    dummy_user.friends = NULL;
    dummy_user.collegues = NULL;
    dummy_user.mailbox = NULL;

    /* initiating users array */
    ASSERT(users = (user_t **)calloc(MEMBERS_INIT_SZ, sizeof(user_t *)), 
	ERRT_ALLOC, ERRA_PANIC, "could not perform memory allocation");

    for (i = 0; i < MEMBERS_INIT_SZ; i++)
	users[i] = DUMMY_USER;
    usr_count = 0;
    usr_array_size = MEMBERS_INIT_SZ;

    /* initiating the daemon socket */
    ASSERT((int)(prtp = getprotobyname(CONN_PROTO_INIT)), ERRT_GETPROTOBYNAME, 
	ERRA_PANIC, "could not map connection protocol to integer");
    memset((char *)&svr_addr, 0, sizeof(sockaddr_in));
    svr_addr.sin_family = AF_INET; /* set family to internet */
    svr_addr.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
    svr_addr.sin_port = htons((u_short)CONN_PORT);

    ASSERT(daemon = socket(PF_INET, SOCK_STREAM, prtp->p_proto), ERRT_SOCKET, 
	ERRA_PANIC, "could not create socket");
    ASSERT(bind(daemon, (sockaddr *)&svr_addr, sizeof(svr_addr)), ERRT_BIND, 
	ERRA_PANIC, "could not bind socket");
    ASSERT(listen(daemon, QLEN), ERRT_LISTEN, ERRA_PANIC, "could not listen "
	"on socket");

    PDMN("up and ready to receive incoming connections");
    return daemon;
}

/* daemon loop function */
static void d_loop(sck_t daemon)
{
    int addr_len;
    sockaddr_in client_addr;
    pthread_t thread;
    svr_t *svr;

    addr_len = sizeof(client_addr);
    while (1)
    {
	ASSERT(svr = alloc_svr(), ERRT_ALLOC, ERRA_WARN, "could not perform "
	    "memory allocation");
	ASSERT(SVR_SCK(svr) = accept(daemon, (sockaddr *)&client_addr,
	    &addr_len), ERRT_ACCEPT, ERRA_WARN, "could not accept on socekt");

	 pthread_create(&thread, NULL, s_handle, svr);
    }
}

int main(int argc, char *argv[])
{
    sck_t daemon = d_init();
    d_loop(daemon);

    return 0;
}
