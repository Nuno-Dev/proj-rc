#ifndef CONSTANTS_H
#define CONSTANTS_H

/* By default GS server is set to be listening on localhost */
#define GS_DEFAULT_IP "127.0.0.1"

/* By default GS server is set to be listening on port 58000 + group number (71) */
#define GS_DEFAULT_PORT "58071"

/* Hostnames (including the dots) can be at most 253 characters long */
#define GS_IP_SIZE 254

/* Ports range from 0 to 65535 */
#define GS_PORT_SIZE 6

/* The maximum buffer size to read from stdin from client side (it's actually around 273 but we'll give a seg fault margin for wrong input) */
#define CLIENT_COMMAND_SIZE 512

/* The maximum number of words written on command in stdin by the client (it's actually around 256 but we'll give a seg fault margin too) */
#define CLIENT_NUMTOKENS 512

/* Macros used to parse client command and use switch cases instead of strcmp */
#define INVALID_COMMAND -1
#define START 1
#define PLAY 2
#define GUESS 3
#define SCOREBOARD 4
#define HINT 5
#define STATE 6
#define QUIT 7
#define EXIT 8
#define KILLGAME 9
#define KILLPDIR 10

/* State of the current client connection */
#define LOGGED_OUT 0
#define LOGGED_IN 1

/* State of the current verbose flag */
#define VERBOSE_ON 1
#define VERBOSE_OFF 0

/* max length of line in word file */
#define MAX_LINE_SIZE 100

/* Max number of tries to recover packets that were sent via UDP protocol */
#define MAX_UDP_RECV_TRIES 3

/* The size of a client's PLID according to the statement's rules */
#define CLIENT_PLID_SIZE 6

#define CLIENT_MESSAGE_UDP_SIZE 40

#define SERVER_MESSAGE_UDP_SIZE 4096

/* The buffer size for a protocol message code */
#define SERVER_COMMAND_SIZE 4

/* The size of a buffer containing a GS message to the client to send the command status */
#define SERVER_TCP_STATUS_SIZE 32

/* The buffer size for a protocol message status via UDP protocol */
#define SERVER_STATUS_SIZE 8

#define MAX_WORD_LENGTH_SIZE 30

#define TCP_MESSAGE_READ_BUFFER_SIZE 512

#define FILE_NAME_SIZE 1024

/* TCP connection flags */
#define ON 1
#define OFF 0

/* GS wrong protocol message */
#define ERROR_MSG "ERR\n"

/* GS wrong protocol message bytes */
#define ERROR_MSG_SIZE 4

/* Maximum number of connections the kernel should queue for this socket */
#define GS_LISTENQUEUE_SIZE 10

/* Game state flags */
#define GAME_WON 2
#define GAME_CONTINUE 1
#define GAME_LOST 0
#define GAME_ERROR -1

#endif