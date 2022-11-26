#include "client-api.h"
#include "../hangman-api-constants.h"
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
int fdTCP;
struct addrinfo hintsTCP, *resTCP;

/* Client current session variables */
int clientSession; // LOGGED_IN or LOGGED_OUT
char activeClientUID[CLIENT_UID_SIZE], activeClientPWD[CLIENT_PWD_SIZE];

/* Client DS group selected variable */
char activeDSGID[DS_GID_SIZE];

/* Message to DS via UDP protocol variable */
char messageToDSUDP[CLIENT_TO_DS_UDP_SIZE];

/**
 ***************************************************************************
 *       UDP connection
 ***************************************************************************
 */
void createUDPSocket()
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
    int errcode = getaddrinfo(GSIP, GSport, &hintsUDP, &resUDP);
    if (errcode != 0)
    {
        perror("[-] Failed on UDP address translation");
        close(fdDSUDP);
        exit(EXIT_FAILURE);
    }
}

void sendUDPMessage(char *message)
{
    int numTries = DEFAULT_UDPRECV_TRIES;
    ssize_t n;

    while (numTries-- > 0)
    {
        // Client -> DS message
        n = sendto(fdDSUDP, message, strlen(message), 0, resUDP->ai_addr, resUDP->ai_addrlen); // strlen counts nl
        if (n == -1)
        { // Syscall failed -> terminate gracefully
            perror("[-] UDP message failed to send");
            failDSUDP();
        }
        // Start recvfrom timer
        if (timerOn(fdDSUDP) == -1)
        {
            perror("[-] Failed to set read timeout on UDP socket");
            failDSUDP();
        }
        // DS -> Client message
        addrlenUDP = sizeof(addrUDP);
        char dsReply[DS_TO_CLIENT_UDP_SIZE];
        n = recvfrom(fdDSUDP, dsReply, DS_TO_CLIENT_UDP_SIZE, 0, (struct sockaddr *)&addrUDP, &addrlenUDP);
        if (n == -1)
        {
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && (numTries > 0))
            {
                continue;
            }
            else
            {
                perror("[-] Failed to receive UDP message");
                failDSUDP();
            }
        }
        // Turn timer off if message was received
        if (timerOff(fdDSUDP) == -1)
        {
            perror("[-] Failed to reset read timeout on UDP socket");
            failDSUDP();
        }
        dsReply[n] = '\0';
        if (dsReply[n - 1] != '\n')
        { // Each request/reply ends with newline according to DS-Client communication protocol
            fprintf(stderr, "[-] Wrong protocol message received from server via UDP. Program will now exit.\n");
            failDSUDP();
        }
        dsReply[n - 1] = '\0'; // Replace \n with \0
        processDSUDPReply(dsReply);
        break;
    }
}

void processUDPReply(char *message)
{
    char codeDS[PROTOCOL_CODE_SIZE], statusDS[PROTOCOL_STATUS_UDP_SIZE];
    sscanf(message, "%s %s", codeDS, statusDS);
    if (!strcmp(codeDS, "RRG"))
    { // REGISTER command
        if (!strcmp(statusDS, "OK"))
        {
            printf("[+] You have successfully registered.\n");
        }
        else if (!strcmp(statusDS, "DUP"))
        {
            fprintf(stderr, "[-] This user is already registered in the DS database.\n");
        }
        else if (!strcmp(statusDS, "NOK"))
        {
            fprintf(stderr, "[-] Something went wrong with register. Please try again later.\n");
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "RUN"))
    { // UNREGISTER command
        if (!strcmp(statusDS, "OK"))
        {
            printf("[+] You have successfully unregistered.\n");
        }
        else if (!strcmp(statusDS, "NOK"))
        {
            fprintf(stderr, "[-] Something went wrong with unregister. Please check user credentials and try again.\n");
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "RLO"))
    { // LOGIN command
        if (!strcmp(statusDS, "OK"))
        {
            clientSession = LOGGED_IN;
            printf("[+] You have successfully logged in.\n");
        }
        else if (!strcmp(statusDS, "NOK"))
        {
            fprintf(stderr, "[-] Something went wrong with login. Please check user credentials and try again.\n");
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "ROU"))
    { // LOGOUT command
        if (!strcmp(statusDS, "OK"))
        {
            clientSession = LOGGED_OUT;
            printf("[+] You have successfully logged out.\n");
        }
        else if (!strcmp(statusDS, "NOK"))
        {
            fprintf(stderr, "[-] Something went wrong with logout. Please try again and/or contact the developers.\n");
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "RGL"))
    { // GROUPS command
        if (isNumber(statusDS))
        {
            displayGroups(message, atoi(statusDS));
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "RGS"))
    { // SUBSCRIBE command
        if (!strcmp(statusDS, "OK"))
        {
            printf("[+] You have successfully subscribed to this group.\n");
        }
        else if (!strcmp(statusDS, "NEW"))
        {
            char newGID[DS_GID_SIZE];
            strncpy(newGID, message + 8, 2); // copy the new group ID : len(RGS NEW ) = 8
            newGID[DS_GID_SIZE - 1] = '\0';
            printf("[+] You have successfully created a new group (ID = %s).\n", newGID);
        }
        else if (!strcmp(statusDS, "E_FULL"))
        {
            fprintf(stderr, "[-] Group creation has failed. Maximum number of groups has been reached.\n");
        }
        else if (!strcmp(statusDS, "E_USR"))
        {
            fprintf(stderr, "[-] UID submitted to server is incorrect. Please try again and/or contact the developers.\n");
        }
        else if (!strcmp(statusDS, "E_GRP"))
        {
            fprintf(stderr, "[-] Group ID submitted is invalid. Please check available groups using 'gl' command and try again.\n");
        }
        else if (!strcmp(statusDS, "E_GNAME"))
        {
            fprintf(stderr, "[-] Group name submitted is invalid. Please try again.\n");
        }
        else if (!strcmp(statusDS, "NOK"))
        {
            fprintf(stderr, "[-] The group subscribing process has failed. Please try again.\n");
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "RGU"))
    { // UNSUBSCRIBE command
        if (!strcmp(statusDS, "OK"))
        {
            printf("[+] You have successfully unsubscribed to this group.\n");
        }
        else if (!strcmp(statusDS, "E_USR"))
        {
            fprintf(stderr, "[-] UID submitted to server is incorrect. Please try again and/or contact the developers.\n");
        }
        else if (!strcmp(statusDS, "E_GRP"))
        {
            fprintf(stderr, "[-] Invalid given group ID. Please check available groups using 'gl' command and try again.\n");
        }
        else
        {
            errDSUDP();
        }
    }
    else if (!strcmp(codeDS, "RGM"))
    { // MY_GROUPS command
        if (isNumber(statusDS))
        {
            displayGroups(message, atoi(statusDS));
        }
        else
        {
            errDSUDP();
        }
    }
    else
    { // Unexpected protocol DS reply was made
        errDSUDP();
    }
}

static void failUDP()
{
    closeUDPSocket(fdDSUDP, resUDP);
    exit(EXIT_FAILURE);
}

static void errUDP()
{
    fprintf(stderr, "[-] Wrong protocol message received from server via UDP. Program will now exit.\n");
    failUDP();
}

/**
 ***************************************************************************
 *       TCP connection
 ***************************************************************************
 */

void connectTCPSocket()
{
    fdTCP = socket(AF_INET, SOCK_STREAM, 0);
    if (fdTCP == -1)
    {
        perror("[-] Client TCP socket failed to create");
        failDSUDP();
    }
    memset(&hintsTCP, 0, sizeof(hintsTCP));
    hintsTCP.ai_family = AF_INET;
    hintsTCP.ai_socktype = SOCK_STREAM;
    int errcode = getaddrinfo(GSIP, GSport, &hintsTCP, &resTCP);
    if (errcode != 0)
    {
        perror("[-] Failed on TCP address translation");
        close(fdTCP);
        failDSUDP();
    }
    int n = connect(fdTCP, resTCP->ai_addr, resTCP->ai_addrlen);
    if (n == -1)
    {
        perror("[-] Failed to connect to TCP socket");
        failDSTCP();
    }
}

static void failTCP()
{
    closeUDPSocket(fdDSUDP, resUDP);
    closeTCPSocket(fdTCP, resTCP);
    exit(EXIT_FAILURE);
}

static void errTCP()
{
    fprintf(stderr, "[-] Wrong protocol message received from server via TCP. Program will now exit.\n");
    failTCP();
}

/**
 ***************************************************************************
 *       Client COMMANDS
 ***************************************************************************
 */

/* Parse client commands */
int parseClientCommand(char *command)
{
    if (!strcmp(command, "start") || !strcmp(command, "sg"))
        return START;
    else if (!strcmp(command, "play") || !strcmp(command, "pl"))
        return PLAY;
    else if (!strcmp(command, "guess") || !strcmp(command, "gw"))
        return GUESS;
    else if (!strcmp(command, "scoreboard") || !strcmp(command, "sb"))
        return SCOREBOARD;
    else if (!strcmp(command, "hint") || !strcmp(command, "h"))
        return HINT;
    else if (!strcmp(command, "state") || !strcmp(command, "st"))
        return STATE;
    else if (!strcmp(command, "quit"))
        return QUIT;
    else if (!strcmp(command, "exit"))
        return EXIT;
    else
    { // No valid command was received
        fprintf(stderr, "[-] Invalid user command code. Please try again.\n");
        return INVALID_COMMAND;
    }
}

void clientStart(char **tokenList, int numTokens);
void clientGuess(char **tokenList, int numTokens);
void clientPlay(char **tokenList, int numTokens);
void clientScoreboard(char **tokenList, int numTokens);
void clientHint(char **tokenList, int numTokens);
void clientState(char **tokenList, int numTokens);
void clientQuit(int numTokens);

void clientExit(int numTokens)
{
    if (numTokens != 1)
    { // EXIT
        fprintf(stderr, "[-] Incorrect exit command usage. Please try again.\n");
        return;
    }
    closeUDPSocket(fdDSUDP, resUDP);
    printf("[+] Exiting...\n");
    exit(EXIT_SUCCESS);
}