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
int fdUDP;
struct addrinfo hintsUDP, *resUDP;
struct sockaddr_in addrUDP;
socklen_t addrlenUDP;

/* TCP Socket related variables */
int fdTCP;
struct addrinfo hintsTCP, *resTCP;

/* Client current session variables */
char clientPLID[CLIENT_PLID_SIZE];

/* Message to DS via UDP protocol variable */
char clientMessage[CLIENT_MESSAGE_UDP_SIZE];

/**
 ***************************************************************************
 *       UDP connection
 ***************************************************************************
 */

void createUDPSocket()
{
    fdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (fdUDP == -1)
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
        close(fdUDP);
        exit(EXIT_FAILURE);
    }
}

void sendUDPMessage(char *message)
{
    int numTries = MAX_UDP_RECV_TRIES;
    ssize_t n;

    while (numTries-- > 0)
    {
        // Client -> GS message
        n = sendto(fdUDP, message, strlen(message), 0, resUDP->ai_addr, resUDP->ai_addrlen);
        if (n == -1)
        { // Syscall failed -> terminate gracefully
            perror("[-] UDP message failed to send");
            failUDP();
        }
        // Start recvfrom timer
        if (timerOn(fdUDP) == -1)
        {
            perror("[-] Failed to set read timeout on UDP socket");
            failUDP();
        }
        // GS -> Client message
        addrlenUDP = sizeof(addrUDP);
        char serverReply[SERVER_MESSAGE_UDP_SIZE];
        n = recvfrom(fdUDP, serverReply, SERVER_MESSAGE_UDP_SIZE, 0, (struct sockaddr *)&addrUDP, &addrlenUDP);
        if (n == -1)
        {
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && (numTries > 0))
                continue;
            perror("[-] Failed to receive UDP message");
            failUDP();
        }
        // Turn timer off if message was received
        if (timerOff(fdUDP) == -1)
        {
            perror("[-] Failed to reset read timeout on UDP socket");
            failUDP();
        }
        serverReply[n] = '\0';
        if (serverReply[n - 1] != '\n')
        { // Each request/reply must end with newline according to the protocol
            fprintf(stderr, "[-] Wrong protocol message received from server via UDP. Program will now exit.\n");
            failUDP();
        }
        serverReply[n - 1] = '\0'; // Remove newline from message
        processUDPReply(serverReply);
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
    closeUDPSocket(fdUDP, resUDP);
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
        failUDP();
    }
    memset(&hintsTCP, 0, sizeof(hintsTCP));
    hintsTCP.ai_family = AF_INET;
    hintsTCP.ai_socktype = SOCK_STREAM;
    int errcode = getaddrinfo(GSIP, GSport, &hintsTCP, &resTCP);
    if (errcode != 0)
    {
        perror("[-] Failed on TCP address translation");
        close(fdTCP);
        failUDP();
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
    closeUDPSocket(fdUDP, resUDP);
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

void clientStart(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    { // start/sg PLID
        fprintf(stderr, "[-] Incorrect start command usage. Please try again.\n");
        return;
    }
    if (!isValidPLID(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "[-] Invalid start command arguments. Please check given PLID and try again.\n");
        return;
    }
    strcpy(clientPLID, tokenList[1]);
    sprintf(clientMessage, "SNG %s\n", tokenList[1]);
    sendUDPMessage(clientMessage);
}

void clientPlay(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    { // play/pl LETTER
        fprintf(stderr, "[-] Incorrect play command usage. Please try again.\n");
        return;
    }
    if (!isValidPlay(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "[-] Invalid play command arguments. Please check given input and try again.\n");
        return;
    }
    sprintf(clientMessage, "PLG %s\n", tokenList[1]);
    sendUDPMessage(clientMessage);
}

void clientGuess(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    { // guess/gw WORD
        fprintf(stderr, "[-] Incorrect guess command usage. Please try again.\n");
        return;
    }
    if (!isValidGuess(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "[-] Invalid guess command arguments. Please check given word and try again.\n");
        return;
    }
    sprintf(clientMessage, "PWG %s\n", tokenList[1]);
    sendUDPMessage(clientMessage);
}

void clientScoreboard(int numTokens)
{
    if (numTokens != 1)
    { // scoreboard/sb
        fprintf(stderr, "[-] Incorrect scoreboard command usage. Please try again.\n");
        return;
    }

    connectTCPSocket();
    sprintf(clientMessage, "GSB\n");
    if (sendTCPMessage(fdTCP, clientMessage) == -1)
    {
        failTCP();
    }

    /******************************************************/
    /* GET RESPONSE FROM SERVER AND PROCESS THAT RESPONSE*/
    /******************************************************/
}

void clientHint(int numTokens)
{
    if (numTokens != 1)
    { // hint/h
        fprintf(stderr, "[-] Incorrect hint command usage. Please try again.\n");
        return;
    }

    connectTCPSocket();
    sprintf(clientMessage, "GHL\n");
    if (sendTCPMessage(fdTCP, clientMessage) == -1)
    {
        failTCP();
    }

    /******************************************************/
    /* GET RESPONSE FROM SERVER AND PROCESS THAT RESPONSE*/
    /******************************************************/
}

void clientState(int numTokens)
{
    if (numTokens != 1)
    { // state/st
        fprintf(stderr, "[-] Incorrect state command usage. Please try again.\n");
        return;
    }

    connectTCPSocket();
    sprintf(clientMessage, "STA\n");
    if (sendTCPMessage(fdTCP, clientMessage) == -1)
    {
        failTCP();
    }

    /******************************************************/
    /* GET RESPONSE FROM SERVER AND PROCESS THAT RESPONSE*/
    /******************************************************/
}

void clientQuit(int numTokens)
{
    if (numTokens != 1)
    { // quit
        fprintf(stderr, "[-] Incorrect quit command usage. Please try again.\n");
        return;
    }
    sprintf(clientMessage, "QUT\n");
    sendUDPMessage(clientMessage);
    closeTCPSocket(fdTCP, resTCP);
    printf("[+] Quitting...\n");
}

void clientExit(int numTokens)
{
    if (numTokens != 1)
    { // exit
        fprintf(stderr, "[-] Incorrect exit command usage. Please try again.\n");
        return;
    }
    sprintf(clientMessage, "QUT\n");
    sendUDPMessage(clientMessage);
    closeTCPSocket(fdTCP, resTCP);
    closeUDPSocket(fdUDP, resUDP);
    printf("[+] Exiting...\n");
    exit(EXIT_SUCCESS);
}