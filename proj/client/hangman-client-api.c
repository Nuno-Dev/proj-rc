#include "hangman-client-api.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* DS Server information variables */
extern char GSIP[GS_IP_SIZE] = GS_DEFAULT_IP;
extern char GSport[GS_PORT_SIZE] = GS_DEFAULT_PORT;

/* UDP Socket related variables */
int fdDSUDP;
struct addrinfo hintsUDP, *resUDP;
struct sockaddr_in addrUDP;
socklen_t addrlenUDP;

/* TCP Socket related variables */
int fdDSTCP;
struct addrinfo hintsTCP, *resTCP;

/* Client current session variables */
int clientSession; // LOGGED_IN or LOGGED_OUT
char activeClientUID[CLIENT_UID_SIZE], activeClientPWD[CLIENT_PWD_SIZE];

/* Client DS group selected variable */
char activeDSGID[DS_GID_SIZE];

/* Message to DS via UDP protocol variable */
char messageToDSUDP[CLIENT_TO_DS_UDP_SIZE];

static void failDSUDP()
{
    closeUDPSocket(fdDSUDP, resUDP);
    exit(EXIT_FAILURE);
}

static void errDSUDP()
{
    fprintf(stderr, "[-] Wrong protocol message received from server via UDP. Program will now exit.\n");
    failDSUDP();
}

static void failDSTCP()
{
    closeUDPSocket(fdDSUDP, resUDP);
    closeTCPSocket(fdDSTCP, resTCP);
    exit(EXIT_FAILURE);
}

static void errDSTCP()
{
    fprintf(stderr, "[-] Wrong protocol message received from server via TCP. Program will now exit.\n");
    failDSTCP();
}

void createDSUDPSocket()
{
    fdDSUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (fdDSUDP == -1)
    {
        perror("[-] Client UDP socket failed to create");
        exit(EXIT_FAILURE);
    }
    memset(&hintsUDP, 0, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_INET;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    int errcode = getaddrinfo(addrDS, portDS, &hintsUDP, &resUDP);
    if (errcode != 0)
    {
        perror("[-] Failed on UDP address translation");
        close(fdDSUDP);
        exit(EXIT_FAILURE);
    }
}