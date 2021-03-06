#define _CLIENT_

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "chat.h"
#include "chat_io.h"
#include "client.h"

#define ASSERT(VALUE, ERROR, ACTION, FMT, ...)                                \
	__ASSERT(((long)(VALUE)), ERROR, ACTION, printf(FMT, ##__VA_ARGS__))

#define STRING_TERMINATOR 0x0
#define ASCII_BACKSPACE   0x8
#define ASCII_TAB         0x9
#define ASCII_NEWLINE     0xa
#define ASCII_SPACE       0x20
#define ASCII_DELETE      0x7f
#define IS_WHITESPACE(X) (((X)==ASCII_TAB) || ((X)==ASCII_SPACE))
#define ASCII_CHAT_SUBSET(X) (0x20 <= (X) && (X) <= 0x7e)

#define PRINT_PROMPT do {                                                     \
	SET_COLOUR(stdout, COLOUR_CLEAR);                                     \
	if (*cname) {                                                         \
		SET_COLOUR(stdout, COLOUR_CLIENT_PROMPT);                     \
		fprintf(stdout, "%s", cname);                                 \
		SET_COLOUR(stdout, COLOUR_CLEAR);                             \
	}                                                                     \
	fprintf(stdout, "> ");                                                \
	SET_COLOUR(stdout, COLOUR_CLIENT);                                    \
	fflush(stdout);                                                       \
} while (0)

#define PRINT_IM(NAME, MSG) do {                                              \
	SET_COLOUR(stdout, COLOUR_CLEAR);                                     \
	fprintf(stdout, "\n");                                                \
	SET_COLOUR(stdout, COLOUR_PEER_PROMPT);                               \
	fprintf(stdout, "%s", NAME);                                          \
	SET_COLOUR(stdout, COLOUR_CLEAR);                                     \
	fprintf(stdout, ": ");                                                \
	SET_COLOUR(stdout, COLOUR_PEER);                                      \
	fprintf(stdout, "%s", MSG);                                           \
	fprintf(stdout, "\n");                                                \
	SET_COLOUR(stdout, COLOUR_CLEAR);                                     \
} while (0)

#define STR_CLOST_CONNECTION "connection with the server has been lost"

#define REPEAT_IP "-"
#define DEFAULT_SVR_NM "localhost"
#define DEFAULT_SVR_IP "127.0.0.1"
#define EMPTY_STRING ""
#define FRIENDS_ONLINE 1
#define FRIENDS_OFFLINE 0

#define EXIT(n) do {                                                          \
	COLOUR_RESET;                                                         \
	exit(n);                                                              \
} while (0)

static cstat_t cstatus;
static sck_t csocket;
static char cname[STR_MAX_NAME_LENGTH];
static char sname[STR_MAX_HOST_NAME_LENGTH];
static char input_param_0[INPUT_PARAM_SZ];
static char input_param_1[INPUT_PARAM_SZ];
static char *input_param[PARAM_LIST_SZ];
static cfriend_t *friends_loggedon;
static cfriend_t *friends_loggedoff;
static cmd_t commands[] = {
	{ CMD_HELP, cmd_help, 1 },
	{ CMD_CONNECT, cmd_connect, 2 },
	{ CMD_DISCONENCT, cmd_disconnect, 1 },
	{ CMD_LOGIN, cmd_login, 4 },
	{ CMD_LOGOUT, cmd_logout, 4 },
	{ CMD_REGISTER, cmd_register, 1 },
	{ CMD_UNREGISTER, cmd_unregister, 1 },
	{ CMD_FRIENDS, cmd_friends, 1 },
	{ CMD_ADD, cmd_add, 1 },
	{ CMD_REMOVE, cmd_remove, 1 },
	{ CMD_IM, cmd_im, 1 },
	{ CMD_CHAT, cmd_chat, 2 },
	{ CMD_EXIT, cmd_exit, 1 },
	{ CMD_YES, cmd_yes, 1 },
	{ CMD_NO, cmd_no, 1 },
	{ EMPTY_STRING, cmd_error, 0 }
};

/* test wether prf is a prefix of str containing at least min characters
 * returns:
 * 0 - if the test is false
 * 1 - if the test is true */
static int c_cmd_prefix(char *prf, char *str, unsigned int min)
{
	int l_prf = strlen(prf);
	int l_str = strlen(str);
	int len = MIN(l_prf, l_str); 

	if ((len < min) || (l_str < l_prf) || strncasecmp(prf, str, len))
		return 0;

	return 1;
}

/* read a string of at most len characters from the standard input and place it
 * in buf
 * the function skips leading white spaces and sets the first character of the 
 * first token, if any, at buf[0]
 * on return buf contains user's line of input without leading white spaces
 * returns:
 *  the number of characters read not including leading white spaces or
 *  -1 if the number of characters read was >= len -1 */
static int c_input_get(char *buf, int len)
{
	int i = 0;
	char c;

	memset(buf, 0, len);

	do {
		c = fgetc(stdin);
	} while (IS_WHITESPACE(c));

	if ((c == ASCII_NEWLINE) || (c == EOF))
		return 0;

	do {
		if (!ASCII_CHAT_SUBSET(c)) {
			if (i)
				i--;
			continue;
		}

		buf[i] = c;
		i++;
	} while (((c = fgetc(stdin)) != ASCII_NEWLINE) && (i < len - 1));

	if (i >= len -1) {
		while (fgetc(stdin) != ASCII_NEWLINE);
		return -1;
	}

	buf[i] = STRING_TERMINATOR;
	return i;
}

static int c_input_parse(char **srcpp, char *dstp, int len)
{
#define IS_TOKEN(X) ((X) && !(IS_WHITESPACE(X))) /* no ASCII_NEWLINE */

	int i = 0;

	memset(dstp, 0, len);
	if (!srcpp || !(*srcpp) || !dstp)
		return -1;

	if (len < 2)
		return IS_TOKEN(**srcpp) ? -1 : 0;

	while (IS_TOKEN(**srcpp) && (i < len - 1)) {
		*dstp = **srcpp;
		(*srcpp) = (*srcpp) + 1;
		dstp++;
		i++;
	}

	if (IS_TOKEN(**srcpp) && (i >= len - 1))
		return -1;

	while (IS_WHITESPACE(**srcpp))
		(*srcpp) = (*srcpp) + 1;

	return i;
}

/* TODO: go over all places this function gets called 
 * c_get_command receives num command options of type cmd_f */
static cmd_f c_get_command(int num, ...)
{
#define CMD_NAME(CMD) commands[CMD].name
#define CMD_TYPE(CMD) commands[CMD].type
#define CMD_PREF(CMD) commands[CMD].prefix

#define READ_PARAM ! 
#define VERIFY_NO_EXTRA_PARAM

#define ASSERT_INPUT(PRED, ESTR, ...) do {                                    \
	if (PRED) {                                                           \
		PRINT_ERROR(ESTR "\n", ##__VA_ARGS__);                        \
		return cmd_error;                                             \
	}                                                                     \
} while (0)

#define ASSERT_PARAM(BUF, LEN, ESTR_NO_PARAM, ESTR_OVERWRITE) do {            \
	tmp = inp;                                                            \
	in_ret_val = c_input_parse(&inp, BUF, LEN);                           \
	ASSERT_INPUT(!in_ret_val, ESTR_NO_PARAM);                             \
	ASSERT_INPUT(in_ret_val == -1, "%s - " ESTR_OVERWRITE, tmp);          \
} while (0)

#define ASSERT_EXTRA_PARAM(BUF, ESTR) do {                                    \
	tmp = inp;                                                            \
	in_ret_val = c_input_parse(&inp, BUF, 0);                             \
	ASSERT_INPUT(in_ret_val == -1, "'%s' - invalid parameter(s) for the " \
		"%s command", tmp, ESTR);                                     \
} while (0)

	va_list al;
	cmd_f cmd;
	int i, in_ret_val;
	char input_buffer[INPUT_BUFFER_SZ],
	     input_command[STR_MAX_COMMAND_LENGTH];
	char *inp = input_buffer, *tmp = NULL;

	/* getting input from user */
	if (!(in_ret_val = c_input_get(input_buffer, INPUT_BUFFER_SZ)))
		return cmd_error;
	ASSERT_INPUT(in_ret_val == -1,
		"input string is too long, you may enter at most %i characters",
		INPUT_BUFFER_SZ);

	/* parse command */
	if ((in_ret_val = c_input_parse(&inp, input_command,
		STR_MAX_COMMAND_LENGTH)) == -1) {
		cmd = cmd_error;
		PRINT_ERROR("message too long");
	} else {
		/* verify that command exists in command list */
		i = 0;
		do {
			cmd = CMD_TYPE(i);
			if ((c_cmd_prefix(input_command, CMD_NAME(i),
				CMD_PREF(i)))) {
				break;
			}
		} while (strcmp(CMD_NAME(i++), EMPTY_STRING));
	}

	/* command does not exist in command list or command length is too
	 * long */
	ASSERT_INPUT(cmd == cmd_error, "%s - undefined command", input_buffer);

	/* verify that command is valid in current context */
	va_start(al, num);
	for (i = 0; i < num; i++) {
		if (cmd == va_arg(al, cmd_f))
			break;
	}
	va_end(al);

	/* always accept help requests */
	/* otherwise command is not valid in currnet context */
	ASSERT_INPUT((i == num) && (cmd != cmd_help),
		"%s - invalid use of command", input_command);

	/* receive parameters */
	switch (cmd) {
	case cmd_connect:
#ifdef DEBUG
		/* server destination must be specified */
		ASSERT_PARAM(input_param[0], STR_MAX_HOST_NAME_LENGTH,
			"a server IP or hostname address must be specified",
			"invalid server IP or hostname address");
#endif
		/* verify no extra parameters */
#ifdef DEBUG
		i = 1;
#else
		i = 0;
#endif
		ASSERT_EXTRA_PARAM(input_param[i], "connect");
		break;
	case cmd_disconnect:
		/* verify no extra parameters */
		ASSERT_EXTRA_PARAM(input_param[0], "disconnect");
		break;
	case cmd_login:
		/* read user name to login with */
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify a user name with which to login",
			"invalid user name");
		/* verify no extra parameters */
		ASSERT_EXTRA_PARAM(input_param[1], "login");
		break;
	case cmd_logout:
		/* verify no extra parameters */
		ASSERT_EXTRA_PARAM(input_param[0], "logout");
		break;
	case cmd_register:
		/* read user name to register with */
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify a user name to register",
			"invalid user name");
		/* verify no extra parameters */
		ASSERT_EXTRA_PARAM(input_param[1], "register");
		break;
	case cmd_unregister:
		/* read user name to register with */
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify a user name to unregister",
			"invalid user name");
		/* verify no extra parameters */
		ASSERT_EXTRA_PARAM(input_param[1], "unregister");
		break;
	case cmd_friends:
		tmp = inp;
		in_ret_val = c_input_parse(&inp, input_param[0],
			STR_MAX_NAME_LENGTH);
		ASSERT_INPUT(in_ret_val == -1, "%s - invalid option", tmp);

		/* show both online and offline friends */
		if (!in_ret_val)
			break;

#define STR_ONLINE "online"
#define STR_OFFLINE "offline"
		if (c_cmd_prefix(input_param[0], STR_ONLINE, 2)) {
			ASSERT_EXTRA_PARAM(input_param[1], "friends online");
			cmd = cmd_fonline;
			break;
		}

		if (c_cmd_prefix(input_param[0], STR_OFFLINE, 2)) {
			ASSERT_EXTRA_PARAM(input_param[1], "friends offline");
			cmd = cmd_foffline;
			break;
		}

		PRINT_ERROR("%s - invalid paramater(s) for the friends " \
			"command\n", input_param[0]);
		cmd = cmd_error;
		break;
	case cmd_add:
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify a name to be added", "invalid user name");
		ASSERT_EXTRA_PARAM(input_param[1], "add");
		break;
	case cmd_remove:
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify a name to be removed",
			"invalid user name");
		ASSERT_EXTRA_PARAM(input_param[1], "remove");
		break;
	case cmd_im:
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify a name to whom to send the instant " \
			"message", "invalid user name");

		memset(input_param[1], 0, STR_MAX_IM_LENGTH);
		strncpy(input_param[1], inp, STR_MAX_IM_LENGTH);
		ASSERT_INPUT(input_param[1][STR_MAX_IM_LENGTH - 1],
			"an instant message is limmited to %i characters",
			STR_MAX_IM_LENGTH - 1);
		break;
	case cmd_chat:
		ASSERT_PARAM(input_param[0], STR_MAX_NAME_LENGTH,
			"must specify the name of a friend with whom to chat",
			"invalid user name");
		ASSERT_EXTRA_PARAM(input_param[1], "chat");
		break;
	case cmd_exit:
		/* verify no extra parameters */
		ASSERT_EXTRA_PARAM(input_param[0], "exit");
		break;
	case cmd_yes:
		/* fall through */
	case cmd_no:
		ASSERT_EXTRA_PARAM(input_param[0], "invalid option");
		break;
	case cmd_error:
		break;
	default:
		/* TODO: sanity check */
		break;
	}

	return cmd;
}

static void c_usage(char *binary)
{
	PRINT_ERROR("usage: %s", binary);
}

static void c_help(void)
{
#define FIELD_LENGTH 30
#define HELP_STATUS_PRINT(FMT, ...) do {                                      \
	SET_COLOUR(stdout, COLOUR_STATUS_PROMPT);                             \
	fprintf(stdout, "\nstatus:");                                         \
	SET_COLOUR(stdout, COLOUR_STATUS);                                    \
	fprintf(stdout, " " FMT "\n", ##__VA_ARGS__);                         \
	SET_COLOUR(stdout, COLOUR_CLEAR);                                     \
} while (0)

#define HELP_CMD_PRINT(CMD, DSC) do {                                         \
	fprintf(stdout, "%s%-*s%s - %s" "\n", COLOUR_HELP_CMD, FIELD_LENGTH,  \
		"> " CMD, COLOUR_HELP, DSC);                                  \
		SET_COLOUR(stdout, COLOUR_CLEAR);                             \
} while (0)

#define AVAILABLE_COMMANDS do {                                               \
	SET_COLOUR(stdout, COLOUR_AVAILABLE_CMDS);                            \
	fprintf(stdout, "available commands are:\n");                         \
	SET_COLOUR(stdout, COLOUR_CLEAR);                                     \
} while (0)

#ifdef DEBUG
#define HELP_CMD_CONNECT do {                                                 \
	HELP_CMD_PRINT("connect <destination>", "connect to the chat "        \
		"network, where <destination> is either:");                   \
	SET_COLOUR(stdout, COLOUR_HELP);                                      \
	fprintf(stdout, "%-*s    %s\n", FIELD_LENGTH, " ", "a valid IP "      \
		"address in dot notation, or");                               \
	SET_COLOUR(stdout, COLOUR_HELP);                                      \
	fprintf(stdout, "%-*s    %s\n", FIELD_LENGTH, " ", "a valid host "    \
		"name (TBD), or");                                            \
	SET_COLOUR(stdout, COLOUR_HELP);                                      \
	fprintf(stdout, "%-*s    %s\n", FIELD_LENGTH, " ", "the character "   \
		"'-': initially localhost, otherwise same as previous "       \
		"destination");                                               \
		SET_COLOUR(stdout, COLOUR_CLEAR);                             \
} while (0)
#else
#define HELP_CMD_CONNECT HELP_CMD_PRINT("connect", "connect to the chat "     \
		"network");
#endif

#define HELP_CMD_REGISTER HELP_CMD_PRINT("register <user_name>", "register a "\
		"new user");
#define HELP_CMD_UNREGISTER HELP_CMD_PRINT("unregister <user_name>",          \
		"unregister an existing user");
#define HELP_CMD_LOGIN HELP_CMD_PRINT("login <user_name>", "login as the "    \
		"specified user");
#define HELP_CMD_LOGOUT HELP_CMD_PRINT("logout", "logout the current user")
#define HELP_CMD_DISCONNECT HELP_CMD_PRINT("disconnect", "disconnect from "   \
		"the chat network");
#define HELP_CMD_EXIT HELP_CMD_PRINT("exit", "exit the program");
#define HELP_CMD_YES HELP_CMD_PRINT("yes", "accept");
#define HELP_CMD_NO HELP_CMD_PRINT("no", "reject");

#define HELP_CMD_FRIENDS HELP_CMD_PRINT("friends [ online | offline ]",       \
	"view your list of friends");
#define HELP_CMD_ADD HELP_CMD_PRINT("add <friend_name>", "add a user to your "\
	"list of friends");
#define HELP_CMD_REM HELP_CMD_PRINT("remove <friend_name>", "remove a user "  \
	"from your list of friends");
#define HELP_CMD_IM HELP_CMD_PRINT("im <friend_name> <message>", "send an "   \
	"instant message");
#define HELP_CMD_CHAT HELP_CMD_PRINT("chat <friend_name>", "chat with a "     \
	"user (TBD)");
#define HELP_CMD_ACCEPT HELP_CMD_PRINT("accept <friend_name>", "accept an "   \
	"incoming chat request");
#define HELP_CMD_REJECT HELP_CMD_PRINT("reject <friend_name>", "reject an "   \
	"incoming chat request");

	switch (cstatus) {
	case CSTAT_CONNECT:
		HELP_STATUS_PRINT("you are currently not connected to the chat "
			"network");
		AVAILABLE_COMMANDS;
		HELP_CMD_CONNECT;
		HELP_CMD_EXIT;
		break;
	case CSTAT_LOGIN:
		HELP_STATUS_PRINT("you are currently connected to the chat " \
			"network");
		AVAILABLE_COMMANDS;
		HELP_CMD_REGISTER;
		HELP_CMD_UNREGISTER;
		HELP_CMD_LOGIN;
		HELP_CMD_DISCONNECT;
		HELP_CMD_EXIT;
		break;
	case CSTAT_EXIT:
		HELP_STATUS_PRINT("you have requested to exit the program");
		AVAILABLE_COMMANDS;
		HELP_CMD_YES;
		HELP_CMD_NO;
		break;
	case CSTAT_DISCONNECT:
		HELP_STATUS_PRINT("you have requested to disconnect from the " \
			"chat network");
		AVAILABLE_COMMANDS;
		HELP_CMD_YES;
		HELP_CMD_NO;
		break;
	case CSTAT_LOOP:
		HELP_STATUS_PRINT("you are logged in as '%s'", cname);
		AVAILABLE_COMMANDS;
		HELP_CMD_FRIENDS;
		HELP_CMD_ADD;
		HELP_CMD_REM;
		HELP_CMD_IM;
		HELP_CMD_CHAT;
		HELP_CMD_LOGOUT;
		HELP_CMD_EXIT;
		break;
	default:
		/* TODO: sanity check */
		break;
	}
}

/* add a friend to the users list of friends */
static int c_cfriend_insert(long fid, int fconnected, char *fname)
{
	cfriend_t *new_friend = NULL, **fptr = NULL;

	if (!(new_friend = (cfriend_t*)calloc(1, sizeof(cfriend_t))))
		return -1;

	new_friend->fid = fid;
	new_friend->fconnected = fconnected;
	strncpy(new_friend->fname, fname, MIN(strlen(fname),
		STR_MAX_NAME_LENGTH));

	fptr = fconnected ? &friends_loggedon : &friends_loggedoff;

	while (*fptr) {
		if (strncmp((*fptr)->fname, fname, STR_MAX_NAME_LENGTH) < 0) {
			fptr = &((*fptr)->next);
			continue;
		}

		break;
	}

	new_friend->next = *fptr;
	*fptr = new_friend;
	return 0;
}

/* delete a friend from the users list of friends */
static void c_cfriend_extract(cfriend_t **fptr)
{
	cfriend_t *tmp = *fptr;

	*fptr = (*fptr)->next;
	free(tmp);
}

/* initialization: setting server's IP address/host name */
static void c_init(int argc, char *argv[])
{
	/* usage abidence */
	if (argc != 1) {
		c_usage(argv[0]);
		cstatus = CSTAT_ERROR;
		return;
	}

	PRINT_INTRO("*****************************************");
	PRINT_INTRO("*                                       *");
	PRINT_INTRO("*          Welcome to IAS-Chat          *");
	PRINT_INTRO("*                %sClient%s%s%s                 *", 
		COLOUR_APP_NAME, COL_RESET, COL_BG_BLACK, COLOUR_CLEAR);

	PRINT_INTRO("*                                       *");
	PRINT_INTRO("*           %c IAS, April 2004           *",
		ASCII_COPYRIGHT);
	PRINT_INTRO("*****************************************");
	PRINT_INTRO(STR_NIL);
	PRINT_INTRO(STR_NIL);
	PRINT_FEEDBACK("type 'help' at any stage for a list of available " \
		"commands");

#ifdef DEBUG
	PRINT_DEBUG("DEBUG mode");
#endif

	input_param[0] = input_param_0;
	input_param[1] = input_param_1;
	memset(cname, 0, STR_MAX_NAME_LENGTH);
	strncpy(sname, DEFAULT_SVR_IP, strlen(DEFAULT_SVR_IP));
	cstatus = CSTAT_CONNECT;
	return;
}

static void c_cfriends_clean(void)
{
	cfriend_t **friend = NULL;

	friend = &friends_loggedon;
	while (*friend)
		c_cfriend_extract(friend);

	friend = &friends_loggedoff;
	while (*friend)
		c_cfriend_extract(friend);
}

static void c_panic(void)
{
	PRINT_FEEDBACK("\nexiting now...");
	close(csocket);
	c_cfriends_clean();

	EXIT(0);
}

static void c_cfriends_init(void)
{
	msg_t in_msg;
	tlv_t type;
	long fid;
	int fconnected;
	char *aptr = NULL;
	int more_friends = 1;

	while (more_friends) {
		ASSERT(msg_recv(&in_msg, csocket), ERRT_RECV, ERRA_PANIC,
			"could not receive friend data\n" STR_CLOST_CONNECTION);
		type = MSG_TYP(&in_msg);

		switch (type) {
		case FRIEND_ADD:
			aptr = MSG_VAL(&in_msg);
			fid = *((long*)aptr);
			aptr += sizeof(long);
			fconnected = *((int*)aptr);
			aptr += sizeof(int);

			ASSERT(c_cfriend_insert(fid, fconnected, aptr),
				ERRT_PEERINSERT, ERRA_WARN,
				"could not allocate memory for friend '%s'",
				aptr);
			break;
		case FRIENDS_END:
			more_friends = 0;
			break;
		case FRIEND_UNREGISTER:
			aptr = MSG_VAL(&in_msg) + sizeof(long);
			PRINT_FEEDBACK("user '%s' has unregistered", aptr);
			break;
		default:
			PRINT_ERROR("could not complete friends " \
				"initialization");
			PRINT_FEEDBACK("disconnecting...");
			close(csocket);
			cstatus = CSTAT_CONNECT;
		}
	}
}

static void c_exit(void)
{
	cmd_f cmd;
	cstat_t status_hook = cstatus;
	msg_t out_msg;

	cstatus = CSTAT_EXIT;
	PRINT_FEEDBACK("are you sure you would like to exit?");

	do {
		do {
			PRINT_PROMPT;
		} while ((cmd = c_get_command(2, cmd_yes, cmd_no)) ==
			cmd_error);

		switch (cmd) {
		case cmd_yes:
			break;
		case cmd_help:
			c_help();
			break;
		case cmd_no:
			cstatus = status_hook;
			return;
		default:
			/* TODO: sanity check */
			break;
		}
	} while (cmd == cmd_help);

	msg_set_data(&out_msg, DISCONNECT, 0, NULL);
	msg_send(&out_msg, csocket);
	if (close(csocket))
		EXIT(1);

	EXIT(0);
}

/* reach this routene in state CSTAT_CONNECT */
static void c_connect(void)
{
	cmd_f cmd;
	protoent *prtp = NULL;
	sockaddr_in cad, sad;
	in_addr saddr;

	do {
		PRINT_PROMPT;
	} while ((cmd = c_get_command(2, cmd_connect, cmd_exit)) == cmd_error);

	switch(cmd) {
	case cmd_connect:
#ifdef DEBUG
#define LOCALHOST_NM "localhost"
#define LOCALHOST_IP "127.0.0.1"
		/* special case destinations */
		/* destination: localhost */
		if (c_cmd_prefix(input_param[0], LOCALHOST_NM, 2)) {
			memset(sname, 0, STR_MAX_HOST_NAME_LENGTH);
			strncpy(sname, LOCALHOST_IP, strlen(LOCALHOST_IP));
		}
		/* not '-': a different destination than in previouse
		 * connection */
		else if (strncmp(input_param[0], REPEAT_IP,
			strlen(REPEAT_IP))) {
			/* saving current destination in case of error */
			memset(input_param[1], 0, STR_MAX_HOST_NAME_LENGTH);
			strncpy(input_param[1], sname, strlen(sname));
			memset(sname, 0, STR_MAX_HOST_NAME_LENGTH);
			strncpy(sname, input_param[0], strlen(input_param[0]));
		}

		/* otherwise: the same destination as in previouse connection:
		 * '-' */
#endif
		/* initiating client socket */
		ASSERT((prtp = getprotobyname(CONN_PROTO_INIT)),
			ERRT_GETPROTOBYNAME, ERRA_PANIC,
			"could not map connection protocol to integer");

		if (!inet_aton(sname, &saddr) /* && gethostbyname */ ) {
			PRINT_ERROR("%s is not a valid IP address or host " \
				"name\n", sname);
#ifdef DEBUG
			memset(sname, 0, STR_MAX_HOST_NAME_LENGTH);
			strncpy(sname, input_param[1], strlen(input_param[1]));
#endif
			return;
		}

		memset(&cad, 0, sizeof(sockaddr_in));
		cad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
		cad.sin_family = AF_INET; /* set family to internet   */
		cad.sin_port = htons((u_short)CONN_PORT); /* set connection
							     port */

		memset(&sad, 0, sizeof(sockaddr_in));
		memcpy(&sad.sin_addr, &saddr, sizeof(in_addr));
		sad.sin_family = AF_INET;
		sad.sin_port = htons((u_short)CONN_PORT);

		ASSERT(csocket = socket(PF_INET, SOCK_STREAM, prtp->p_proto), 
			ERRT_SOCKET, ERRA_PANIC, "could not create socket");

		if (connect(csocket, (sockaddr*)&sad, sizeof(sad)) < 0) {
			PRINT_ERROR("could not connect to %s\n", sname);
			return;
		}

		PRINT_FEEDBACK("conencted to %s", sname);
		cstatus = CSTAT_LOGIN;
		break;
	case cmd_help:
		c_help();
		break;
	case cmd_exit:
		c_exit();
		break;
	default:
		/* TODO: sanity check */
		break;
	}
}

static void c_disconnect(void)
{
	cmd_f cmd;
	msg_t out_msg;
	cstat_t status_hook = cstatus;

	cstatus = CSTAT_DISCONNECT;
	PRINT_FEEDBACK("are you sure you would like to disconnect?");

	do {
		do {
			PRINT_PROMPT;
		} while ((cmd = c_get_command(2, cmd_yes, cmd_no)) ==
			cmd_error);

		switch (cmd) {
		case cmd_yes:
			break;
		case cmd_help:
			c_help();
			break;
		case cmd_no:
			cstatus = status_hook;
			return;
		default:
			/* TODO: sanity check */
			break;
		}
	} while (cmd == cmd_help);

	msg_set_data(&out_msg, DISCONNECT, 0, NULL);
	msg_send(&out_msg, csocket);
	close(csocket);

	cstatus = CSTAT_CONNECT;
}

static void c_register(char *name)
{
	msg_t msg;

	msg_set_data(&msg, REGISTER, strlen(name), name);
	ASSERT(msg_send(&msg, csocket), ERRT_SEND, ERRA_PANIC,
		"could not send registration request\n" STR_CLOST_CONNECTION);

	ASSERT(msg_recv(&msg, csocket), ERRT_RECV, ERRA_PANIC,
		"could not complete registration\n" STR_CLOST_CONNECTION);

	switch (MSG_TYP(&msg)) {
	case REGISTER_SUCCESS:
		PRINT_FEEDBACK("user '%s' was successfuly registered", name);
		break;
	case REGISTER_FAIL_RESERVED:
		PRINT_ERROR("'%s' is reserved and can not be registered as a " \
			"user", name);
		break;
	case REGISTER_FAIL_REREGISTER:
		PRINT_ERROR("user '%s' is already registered, can not " \
			"re-register", name);
		break;
	case REGISTER_FAIL_CAPACITY:
		PRINT_ERROR("user capacity is full, '%s' could not be " \
			"registered", name);
		break;
	default:
		/* TODO: sanity check*/
		break;
	}
}

static void c_unregister(char *name)
{
	msg_t msg;

	msg_set_data(&msg, UNREGISTER, strlen(name), name);
	ASSERT(msg_send(&msg, csocket), ERRT_SEND, ERRA_PANIC,
		"could not send unregistration request\n" STR_CLOST_CONNECTION);

	ASSERT(msg_recv(&msg, csocket), ERRT_RECV, ERRA_PANIC,
		"could not complete unregistration\n" STR_CLOST_CONNECTION);

	if (MSG_TYP(&msg) == UNREGISTER_SUCCESS)
		PRINT_FEEDBACK("user '%s' was successfuly unregistered", name);
	else
		PRINT_ERROR("user '%s' could not be unregistered", name);
}

static void c_login(void)
{
	cmd_f cmd;
	msg_t out_msg, in_msg;

	do {
		PRINT_PROMPT;
	} while ((cmd = c_get_command(5, cmd_login, cmd_register,
		cmd_unregister, cmd_disconnect, cmd_exit)) == cmd_error);

	switch(cmd) {
	case cmd_login:
		msg_set_data(&out_msg, LOGIN, strlen(input_param[0]),
			input_param[0]);
		ASSERT(msg_send(&out_msg, csocket), ERRT_SEND, ERRA_PANIC,
			"could not send a login request\n"
			STR_CLOST_CONNECTION);
		ASSERT(msg_recv(&in_msg, csocket), ERRT_RECV, ERRA_PANIC,
			"could not complete unregistration\n"
			STR_CLOST_CONNECTION);

		switch(MSG_TYP(&in_msg)) {
		case LOGIN_SUCCESS:
			strncpy(cname, input_param[0], STR_MAX_NAME_LENGTH);
			PRINT_FEEDBACK("user '%s' logged in successfuly",
				cname);
			cstatus = CSTAT_LOOP;
			break;
		case LOGIN_LOGGEDIN:
			PRINT_FEEDBACK("user '%s' is already logged in, " \
				"can not re-login", input_param[0]);
			break;
		case LOGIN_FAIL:
			PRINT_FEEDBACK("user '%s' is not registered, " \
				"must first register", input_param[0]);
			break;
		default:
			/* TODO: sanity check*/
			break;
		}

		break;
	case cmd_register:
		c_register(input_param[0]);
		break;
	case cmd_unregister:
		c_unregister(input_param[0]);
		break;
	case cmd_disconnect:
		c_disconnect();
		break;
	case cmd_help:
		c_help();
		break;
	case cmd_exit:
		c_exit();
		break;
	default:
		/* TODO: sanity check */
		break;
	}
}

static void c_logout(void)
{
	msg_t out_msg;

	c_cfriends_clean();
	msg_set_data(&out_msg, LOGOUT, 0, NULL);
	ASSERT(msg_send(&out_msg, csocket), ERRT_SEND, ERRA_PANIC,
		"could not send a login request\n STR_CLOST_CONNECTION");

	PRINT_FEEDBACK("user '%s' logged out succesfully", cname);
	cstatus = CSTAT_LOGIN;
}

static void c_send_im(void)
{
	cfriend_t *friend = NULL;
	msg_t out_msg;
	char buffer[INPUT_BUFFER_SZ];
	int i, text_len;

	if (!strncmp(cname, input_param[0], STR_MAX_NAME_LENGTH)) {
		PRINT_ERROR("can't send yourself an instant message");
		return;
	}

	for (i = 0; i < 2; i++) {
		friend = i ? friends_loggedon : friends_loggedoff;

		while (friend) {
			if (strncmp(friend->fname, input_param[0],
				STR_MAX_NAME_LENGTH)) {
				friend = friend->next;
				continue;
			}

			break;
		}

		if (friend)
			break;
	}

	if (!friend) {
		PRINT_FEEDBACK("user '%s' is not on your list of friends", 
			input_param[0]);
		return;
	}

	if (!i) {
		PRINT_FEEDBACK("user '%s' is currently offline, " \
			"try again later",  input_param[0]);
		return;
	}

	/* TODO: check coppied length for bug */

	memset(buffer, 0, INPUT_BUFFER_SZ);
	memcpy(buffer, &(friend->fid), sizeof(long));
	strncpy(buffer + sizeof(long), input_param[1], text_len = 
		MIN(strlen(input_param[1]), INPUT_BUFFER_SZ - sizeof(long)));
	msg_set_data(&out_msg, IM, text_len + sizeof(long), buffer);

	ASSERT(msg_send(&out_msg, csocket), ERRT_SEND, ERRA_PANIC, "could not "
		"send an im\n" STR_CLOST_CONNECTION);
}

static void c_send_chat(void)
{
	PRINT_FEEDBACK("...coming soon !!!");
}

static void c_show_friends(int status)
{
	cfriend_t *fptr = (status == FRIENDS_ONLINE) ? friends_loggedon :
		friends_loggedoff;

	PRINT_FEEDBACK("%s:",
		(status == FRIENDS_ONLINE) ? "online" : "offline");

	if (!fptr) {
		SET_COLOUR(stdout, COLOUR_CLEAR);
		fprintf(stdout, " none\n");
	} else {
		SET_COLOUR(stdout, (status == FRIENDS_ONLINE) ?
			COL_BRIGHT_CYAN : COL_BRIGHT_RED);
		do {
			printf(" %s\n", fptr->fname);
			fptr = fptr->next;
		} while (fptr);

		SET_COLOUR(stdout, COLOUR_CLEAR);
	}
}

static int c_validate_new_friend(cfriend_t *flist, char *name)
{
	while (flist) {
		if (!strncmp(flist->fname, name, STR_MAX_NAME_LENGTH)) {
			PRINT_FEEDBACK("users '%s' is already in your list " \
				"of friends", flist->fname);
			return -1;
		}

		flist = flist->next;
	}

	return 0;
}

static void c_add_friend(void)
{
	msg_t in_msg, out_msg;
	long fid;
	int fconnected, ret;
	char *aptr = NULL;

	if (!strncmp(cname, input_param[0], STR_MAX_NAME_LENGTH)) {
		PRINT_ERROR("can't add yourself to your list of friends");
		return;
	}

	if (c_validate_new_friend(friends_loggedon, input_param[0]) ||
		c_validate_new_friend(friends_loggedoff, input_param[0])) {
		return;
	}

	msg_set_data(&out_msg, FRIEND_ADD, strlen(input_param[0]),
		input_param[0]);
	ASSERT(msg_send(&out_msg, csocket), ERRT_SEND, ERRA_PANIC,
		"could not add a new friend\n" STR_CLOST_CONNECTION);
	ASSERT(msg_recv(&in_msg, csocket), ERRT_RECV, ERRA_PANIC,
		"could not receive friend information\n" STR_CLOST_CONNECTION);

	switch (MSG_TYP(&in_msg)) {
	case FRIEND_ADD_SUCCESS:
		aptr = MSG_VAL(&in_msg);
		fid = *((long*)aptr);
#if 0
		aptr += sizeof(long);
		fconnected = *((int*)aptr);
#endif
		ASSERT(ret = c_cfriend_insert(fid, fconnected = 0,
			input_param[0]), ERRT_PEERINSERT, ERRA_WARN,
			"could not allocate memory for friend  '%s'",
			input_param[0]);

		if (!ret) {
			PRINT_FEEDBACK("user '%s' has been added to your of " \
				"list friends", input_param[0]);
#if 0
			PCLIENT("user '%s' is currently %s", input_param[0],
				fconnected ? "online" : "offline");
#endif
		} else {
			PRINT_ERROR("couldn't add '%s' to your list o friends", 
				input_param[0]);
		}
		break;
	case FRIEND_ADD_FAIL:
		PRINT_FEEDBACK("user '%s' is not registered", input_param[0]);
		break;
	default:
#ifdef DEBUG
		PRINT_DEBUG("received a bad message:\n");
		PRINT_DEBUG("MSG_TYP(&in_msg): %i\n", MSG_TYP(&in_msg));
		PRINT_DEBUG("MSG_LEN(&in_msg): %i\n", MSG_LEN(&in_msg));
#endif
		break;
	}
}

static void c_remove_friend(void)
{
	cfriend_t **fptr = NULL;
	msg_t out_msg;
	int i;

	for (i = 0; i < 2; i++) {
		fptr = i ? &friends_loggedon : &friends_loggedoff;

		while (*fptr) {
			if (!strncmp((*fptr)->fname, input_param[0],
				STR_MAX_NAME_LENGTH)) {
				break;
			}

			fptr = &((*fptr)->next);
		}

		if (*fptr)
			break;
	}

	if (!(*fptr)) {
		PRINT_FEEDBACK("user '%s' is not in your list of friends", 
			input_param[0]);
		return;
	}

	msg_set_data(&out_msg, FRIEND_REMOVE, sizeof(long), &((*fptr)->fid));
	ASSERT(msg_send(&out_msg, csocket), ERRT_SEND, ERRA_PANIC,
		"could not remove a friend\n" STR_CLOST_CONNECTION);

	c_cfriend_extract(fptr);
	PRINT_FEEDBACK("user '%s' has been removed from your list of friends",
		input_param[0]);
}

static void c_receive_im(char *buffer)
{
	char *nameptr = NULL, *msgptr = NULL;

	nameptr = buffer;
	msgptr = buffer + strlen(nameptr) + 1;
	PRINT_IM(nameptr, msgptr);
}

static int c_cfriend_change_status(long fid, int status)
{
	cfriend_t *temp = NULL, **friend = NULL, **new_list = NULL;

	switch (status) {
		/* friend has come on line */
	case FRIENDS_ONLINE:
		friend = &friends_loggedoff;
		new_list = &friends_loggedon;
		break;
		/* friend has gone off line */
	case FRIENDS_OFFLINE:
		friend = &friends_loggedon;
		new_list = &friends_loggedoff;
		break;
	default:
		/* TODO: sanity check */
		return -1;
		break;
	}

	while (*friend) {
		if ((*friend)->fid != fid) {
			friend = &((*friend)->next);
			continue;
		}

		break;
	}

	if (!(*friend))
		return -1;

	while (*new_list) {
		if (strncmp((*new_list)->fname, (*friend)->fname,
			STR_MAX_NAME_LENGTH) < 0) {
			new_list = &((*new_list)->next);
			continue;
		}

		break;
	}

	temp = *friend;
	*friend = (*friend)->next;
	temp->next = *new_list;
	*new_list = temp;

	PRINT_FEEDBACK("\nuser '%s' has logged %s", temp->fname, 
		(status == FRIENDS_ONLINE) ? "in" : "out");

	return 0;
}

static void c_cfriend_unregister(long fid)
{
	cfriend_t *temp = NULL, **friend = NULL;
	int i;

	for (i = 0; i < 2; i++) {
		friend = i ? &friends_loggedon : &friends_loggedoff;

		while (*friend) {
			if ((*friend)->fid == fid)
				break;
			friend = &(*friend)->next;
		}

		if (*friend)
			break;
	}

	if (!(*friend))
		return;

	temp = *friend;
	*friend = (*friend)->next;

	PRINT_FEEDBACK("\nuser '%s' has unregistered", temp->fname);
	free(temp);
}

static void c_announce_collegue_add(char *name)
{
	PRINT_FEEDBACK("\nuser '%s' has added you as a friend", name);
}

static void c_loop(void)
{
#define STDIN 0

	fd_set rfds;
	msg_t in_msg;
	int loop = 1;

	c_cfriends_init();
	PRINT_FEEDBACK("user '%s' is ready to chat!", cname);

	while (loop) {
		FD_ZERO(&rfds);
		FD_SET(csocket, &rfds);
		FD_SET(0, &rfds);

		PRINT_PROMPT;
		select(MAX(STDIN, csocket) + 1, &rfds, NULL, NULL, NULL);

		if (FD_ISSET(STDIN, &rfds)) {
			switch (c_get_command(7, cmd_im, cmd_chat, cmd_friends,
				cmd_add, cmd_remove, cmd_logout, cmd_exit)) {
			case cmd_im:
				c_send_im();
				break;
			case cmd_chat:
				c_send_chat();
				/* TODO: */
				break;
			case cmd_friends:
				c_show_friends(FRIENDS_ONLINE);
				c_show_friends(FRIENDS_OFFLINE);
				break;
			case cmd_fonline:
				c_show_friends(FRIENDS_ONLINE);
				break;
			case cmd_foffline:
				c_show_friends(FRIENDS_OFFLINE);
				break;
			case cmd_add:
				c_add_friend();
				break;
			case cmd_remove:
				c_remove_friend();
				break;
			case cmd_logout:
				c_logout();
				memset(cname, 0, STR_MAX_NAME_LENGTH);
				return;
			case cmd_exit:
				c_exit();
				break;
			case cmd_help:
				c_help();
				break;
			default:
				/* TODO: sanity check*/
				break;
			}
		}

		if (FD_ISSET(csocket, &rfds)) {
			ASSERT(msg_recv(&in_msg, csocket), ERRT_RECV,
				ERRA_PANIC, "could not receive data from the " \
				"server\n" STR_CLOST_CONNECTION);
			switch (MSG_TYP(&in_msg)) {
			case IM:
				c_receive_im(MSG_VAL(&in_msg));
				break;
			case CHAT_REQ:
				/* TODO: */
				break;
			case CHAT_ACC:
				/* TODO: */
				break;
			case CHAT_REJ:
				/* TODO: */
				break;
			case FRIEND_LOGIN:
				c_cfriend_change_status(
					*((long*)MSG_VAL(&in_msg)),
					FRIENDS_ONLINE);
				break;
			case FRIEND_LOGOUT:
				c_cfriend_change_status(
					*((long*)MSG_VAL(&in_msg)),
					FRIENDS_OFFLINE);
				break;
			case FRIEND_UNREGISTER:
				c_cfriend_unregister(
					*((long*)MSG_VAL(&in_msg)));
				break;
			case COLLEGUE_ADD:
				c_announce_collegue_add(
					MSG_VAL(&in_msg) + sizeof(long));
				break;
			default:
#ifdef DEGUB
				PRINT_DEBUG("received a bad message:\n");
				PRINT_DEBUG("MSG_TYP(&in_msg): %i\n",
					MSG_TYP(&in_msg));
				PRINT_DEBUG("MSG_LEN(&in_msg): %i\n",
					MSG_LEN(&in_msg));
#endif
				break;
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int run = 1;

	c_init(argc, argv);

	while (run) {
		switch (cstatus) {
		case CSTAT_CONNECT:
			c_connect();
			break;
		case CSTAT_LOGIN:
			c_login();
			break;
		case CSTAT_LOOP:
			c_loop();
			break;
		case CSTAT_EXIT:
			c_exit();
			break;
		case CSTAT_ERROR:
			EXIT(1);
		default:
			/* TODO: sanity check */
			break;
		}
	}

	return 0;
}

