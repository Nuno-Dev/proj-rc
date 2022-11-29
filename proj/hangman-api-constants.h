#ifndef HANGMAN_API_CONSTANTS_H
#define HANGMAN_API_CONSTANTS_H

/* Preprocessed macro to determine min(x,y) */
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* By default GS server is set to be listening on localhost */
#define GS_DEFAULT_IP "127.0.0.1"

// ALTERAR AQUI ADICIONAR NUMERO DO GRUPO
/* By default GS server is set to be listening on port 58000 + group number */
#define GS_DEFAULT_PORT "58000" // + GROUP NUMBER

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

/* Max number of tries to recover packets that were sent via UDP protocol */
#define MAX_UDP_RECV_TRIES 3

/* The size of a client's PLID according to the statement's rules */
#define CLIENT_PLID_SIZE 6

#define CLIENT_MESSAGE_UDP_SIZE 40

#define SERVER_MESSAGE_UDP_SIZE 4096

/* The buffer size for a protocol message code */
#define SERVER_COMMAND_SIZE 4

/* The buffer size for a protocol message status via UDP protocol */
#define SERVER_STATUS_SIZE 8

#define CLIENT_PUID_SIZE 6

#define MAX_WORD_LENGTH_SIZE 30

/* TCP connection */
#define ON 1

#define OFF 0

#endif