#ifndef _CLIENT_H_
#define _CLIENT_H_

#define STR_IP_ADDR_LEN 16
#define STR_LOCAL_HOST "127.0.0.1"

#define CMD_HELP "help"
#define CMD_CONNECT "connect"
#define CMD_DISCONENCT "disconnect"
#define CMD_LOGIN "login"
#define CMD_LOGOUT "logout"
#define CMD_REGISTER "register"
#define CMD_UNREGISTER "unregister"
#define CMD_FRIENDS "friends"
#define CMD_ADD "add"
#define CMD_REMOVE "remove"
#define CMD_IM "im"
#define CMD_CHAT "chat"
#define CMD_EXIT "exit"
#define CMD_YES "yes"
#define CMD_NO "no"

typedef enum {
    cmd_help,
    cmd_connect,
    cmd_disconnect,
    cmd_login,
    cmd_logout,
    cmd_register,
    cmd_unregister,
    cmd_friends,
    cmd_fonline,
    cmd_foffline,
    cmd_add,
    cmd_remove,
    cmd_im,
    cmd_chat,
    cmd_accept,
    cmd_reject,
    cmd_exit,
    cmd_yes,
    cmd_no,
    cmd_error,
} cmd_f;

typedef struct {
    char name[STR_MAX_COMMAND_LENGTH];
    cmd_f type;
    int prefix;
} cmd_t;


typedef enum {
    CSTAT_CONNECT,
    CSTAT_DISCONNECT,
    CSTAT_LOGIN,
    CSTAT_LOGOUT,
    CSTAT_LOOP,
    CSTAT_HELP,
    CSTAT_EXIT,
    CSTAT_ERROR,
} cstat_t;


#endif
