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
char GSIP[GS_IP_SIZE] = "tejo.tecnico.ulisboa.pt"; // GS_DEFAULT_IP;
char GSport[GS_PORT_SIZE] = "58011";               // GS_DEFAULT_PORT;

/* UDP Socket related variables */
int fdUDP;
struct addrinfo hintsUDP, *resUDP;
struct sockaddr_in addrUDP;
socklen_t addrlenUDP;

/* TCP Socket related variables */
int fdTCP;
struct addrinfo hintsTCP, *resTCP;

/* Client current session variables */
int clientSession = LOGGED_OUT;
char clientPLID[CLIENT_PLID_SIZE];

/* Message to DS via UDP protocol variable */
char clientMessage[CLIENT_MESSAGE_UDP_SIZE] = {'\0'};

char currentWord[MAX_WORD_LENGTH_SIZE] = {'\0'};

char wordWithSpaces[MAX_WORD_LENGTH_SIZE] = {'\0'};

int wordLength;

int maxNumberTries;

int currentTrial = 1;

char letterGuess[1];

static void failUDP();

static void errUDP();

static void failTCP();

// static void errTCP();

// function to return currentWord with spaces between them
char *getCurrentWordWithSpaces()
{
    int i;
    for (i = 0; i < wordLength; i++)
    {
        wordWithSpaces[i * 2] = currentWord[i];
        wordWithSpaces[i * 2 + 1] = ' ';
    }
    wordWithSpaces[wordLength * 2] = '\0';
    return wordWithSpaces;
}

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
        perror("Client UDP socket failed to create");
        exit(EXIT_FAILURE);
    }
    memset(&hintsUDP, 0, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_INET;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    int errcode = getaddrinfo(GSIP, GSport, &hintsUDP, &resUDP);
    if (errcode != 0)
    {
        perror("Failed on UDP address translation");
        close(fdUDP);
        exit(EXIT_FAILURE);
    }
}

void sendUDPMessage(char *message, int command)
{
    int numTries = MAX_UDP_RECV_TRIES;
    printf("[DEBUG] Sending message to server: %s, size %ld\n", message, strlen(message));
    ssize_t n;
    while (numTries-- > 0)
    {
        // Client -> GS message
        n = sendto(fdUDP, message, strlen(message), 0, resUDP->ai_addr, resUDP->ai_addrlen);
        if (n == -1)
        { // Syscall failed -> terminate gracefully
            perror("UDP message failed to send");
            failUDP();
        }
        // dont wait for reply from GS if command == KILLGAME or KILLPDIR
        if (command == KILLGAME || command == KILLPDIR)
            return;
        // Start recvfrom timer
        if (timerOn(fdUDP) == -1)
        {
            perror("Failed to set read timeout on UDP socket");
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
            perror("Failed to receive UDP message");
            failUDP();
        }
        // Turn timer off if message was received
        if (timerOff(fdUDP) == -1)
        {
            perror("Failed to reset read timeout on UDP socket");
            failUDP();
        }
        serverReply[n] = '\0';
        if (serverReply[n - 1] != '\n')
        { // Each request/reply must end with newline according to the protocol
            fprintf(stderr, "Wrong protocol message received from server via UDP. Program will now exit.\n");
            failUDP();
        }
        serverReply[n - 1] = '\0'; // Remove newline from message
        processUDPReply(serverReply);
        break;
    }
}

void processUDPReply(char *message)
{
    char serverCommand[SERVER_COMMAND_SIZE], serverStatus[SERVER_STATUS_SIZE];
    sscanf(message, "%s %s", serverCommand, serverStatus);
    printf("[DEBUG] Server reply: %s\n", message);

    if (!strcmp(serverCommand, "RSG"))
    { // START response
        if (!strcmp(serverStatus, "OK"))
        {
            clientSession = LOGGED_IN;
            sscanf(message, "%*s %*s %d %d", &wordLength, &maxNumberTries);
            for (int i = 0; i < wordLength; i++)
            {
                currentWord[i] = '_';
            }
            currentWord[wordLength] = '\0';
            printf("New game started (max %d errors): %s\n", maxNumberTries, getCurrentWordWithSpaces());
        }
        else if (!strcmp(serverStatus, "NOK"))
        {
            fprintf(stderr, "Something went wrong with start. Please try again later.\n");
        }
        else
        {
            errUDP();
        }
    }
    else if (!strcmp(serverCommand, "RLG"))
    { // Play response
        if (!strcmp(serverStatus, "OK"))
        {
            int n;
            sscanf(message, "%*s %*s %d %d", &currentTrial, &n);
            // get pos1 pos2 ... posn from "RLG status trial n pos1 pos2 ... posn"
            char *pos = strtok(message, " ");
            // remove 4 first tokens
            for (int i = 0; i < 4; i++)
            {
                pos = strtok(NULL, " ");
            }
            // get all positions and store currentLetter in those positions of wordGuess
            for (int i = 0; i < n; i++)
            {
                int position = atoi(pos) - 1; // -1 because of 0-based indexing
                currentWord[position] = letterGuess[0];
                pos = strtok(NULL, " ");
            }
            currentTrial++;
            printf("Yes, '%c' is part of the word: %s\n", letterGuess[0], getCurrentWordWithSpaces());
        }
        else if (!strcmp(serverStatus, "WIN"))
        {
            // iterate through wordGuess and if the letter is '_' then change it to letterGuess
            for (int i = 0; i < wordLength; i++)
            {
                if (currentWord[i] == '_')
                {
                    currentWord[i] = letterGuess[0];
                }
            }
            printf("WELL DONE! You guessed: %s\n", getCurrentWordWithSpaces());
            clientSession = LOGGED_OUT;
            memset(clientPLID, 0, sizeof(clientPLID));
        }
        else if (!strcmp(serverStatus, "DUP"))
        {
            printf("The letter you played is a duplicate. Please try again with a different letter.\n");
        }
        else if (!strcmp(serverStatus, "NOK"))
        {
            printf("No, '%c' is not part of the word: %s\n", letterGuess[0], getCurrentWordWithSpaces());
            currentTrial++;
        }
        else if (!strcmp(serverStatus, "OVR"))
        {
            printf("No, '%c' is not part of the word: %s\n", letterGuess[0], getCurrentWordWithSpaces());
            printf("GAME OVER! You lost.");
            closeTCPSocket(fdTCP, resTCP);
            clientSession = LOGGED_OUT;
            memset(clientPLID, 0, sizeof(clientPLID));
        }
        else if (!strcmp(serverStatus, "INV"))
        {
            printf("The trial number that you entered is not valid or you have already played a different letter for that trial. Please try again.\n");
        }
        else
        {
            errUDP();
        }
    }
    else
    { // Unexpected protocol DS reply was made
        errUDP();
    }
}

static void failUDP()
{
    closeUDPSocket(fdUDP, resUDP);
    exit(EXIT_FAILURE);
}

static void errUDP()
{
    fprintf(stderr, "Wrong protocol message received from server via UDP. Program will now exit.\n");
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
        perror("Client TCP socket failed to create");
        failUDP();
    }
    memset(&hintsTCP, 0, sizeof(hintsTCP));
    hintsTCP.ai_family = AF_INET;
    hintsTCP.ai_socktype = SOCK_STREAM;
    int errcode = getaddrinfo(GSIP, GSport, &hintsTCP, &resTCP);
    if (errcode != 0)
    {
        perror("Failed on TCP address translation");
        close(fdTCP);
        failUDP();
    }
    int n = connect(fdTCP, resTCP->ai_addr, resTCP->ai_addrlen);
    if (n == -1)
    {
        perror("Failed to connect to TCP socket");
        failTCP();
    }
}

static void failTCP()
{
    closeUDPSocket(fdUDP, resUDP);
    closeTCPSocket(fdTCP, resTCP);
    exit(EXIT_FAILURE);
}

/*static void errTCP()
{
    fprintf(stderr, "Wrong protocol message received from server via TCP. Program will now exit.\n");
    failTCP();
}*/

/**
 ***************************************************************************
 *       Client COMMANDS
 ***************************************************************************
 */

void clientStart(char **tokenList, int numTokens)
{
    if (clientSession == LOGGED_IN)
    {
        printf("You already have a playing session undergoing. Use command <play>.\n");
        return;
    }
    if (numTokens != 2)
    { // start/sg PLID
        fprintf(stderr, "Incorrect start command usage. Please try again.\n");
        return;
    }
    if (!isValidPLID(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "Invalid start command arguments. Please check given PLID and try again.\n");
        return;
    }
    currentTrial = 1;
    strcpy(clientPLID, tokenList[1]);
    sprintf(clientMessage, "SNG %s\n", tokenList[1]);
    sendUDPMessage(clientMessage, START);
}

void clientPlay(char **tokenList, int numTokens)
{
    if (clientSession == LOGGED_OUT)
    {
        printf("You don't have a current gaming session. Use command <start> to start a new session.\n");
        return;
    }

    if (numTokens != 2)
    { // play/pl LETTER
        fprintf(stderr, "Incorrect play command usage. Please try again.\n");
        return;
    }
    if (!isValidPlay(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "Invalid play command arguments. Please check given input and try again.\n");
        return;
    }
    // PLG PLID letter trial
    strcpy(letterGuess, tokenList[1]);
    sprintf(clientMessage, "PLG %s %s %d\n", clientPLID, tokenList[1], currentTrial);
    sendUDPMessage(clientMessage, PLAY);
}

void clientGuess(char **tokenList, int numTokens)
{
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You're not playing any session. Please try again.\n");
        return;
    }
    if (numTokens != 2)
    { // guess/gw WORD
        fprintf(stderr, "Incorrect guess command usage. Please try again.\n");
        return;
    }
    if (!isValidGuess(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "Invalid guess command arguments. Please check given word and try again.\n");
        return;
    }
    strcpy(letterGuess, tokenList[1]);
    sprintf(clientMessage, "PWG %s\n", tokenList[1]);
    sendUDPMessage(clientMessage, GUESS);
}

void clientScoreboard(int numTokens)
{
    if (numTokens != 1)
    { // scoreboard/sb
        fprintf(stderr, "Incorrect scoreboard command usage. Please try again.\n");
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
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You're not playing any session. Please try again.\n");
        return;
    }
    if (numTokens != 1)
    { // hint/h
        fprintf(stderr, "Incorrect hint command usage. Please try again.\n");
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
        fprintf(stderr, "Incorrect state command usage. Please try again.\n");
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
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You're not playing any session. Please try again.\n");
        return;
    }
    if (numTokens != 1)
    { // quit
        fprintf(stderr, "Incorrect quit command usage. Please try again.\n");
        return;
    }
    sprintf(clientMessage, "QUT\n");
    sendUDPMessage(clientMessage, QUIT);
    closeTCPSocket(fdTCP, resTCP);
    if (clientSession == LOGGED_OUT)
    { // The exchangeDSUDPMsg function sets the client session to LOGGED_OUT if reply is OK
        memset(clientPLID, 0, sizeof(clientPLID));
    }
    printf("Quitting...\n");
}

void clientExit(int numTokens)
{
    if (numTokens != 1)
    { // exit
        fprintf(stderr, "Incorrect exit command usage. Please try again.\n");
        return;
    }
    sprintf(clientMessage, "QUT\n");
    sendUDPMessage(clientMessage, EXIT);
    closeTCPSocket(fdTCP, resTCP);
    closeUDPSocket(fdUDP, resUDP);
    printf("Exiting...\n");
    exit(EXIT_SUCCESS);
}

void clientKillGame(int numTokens)
{
    if (numTokens != 1)
    { // killgame
        fprintf(stderr, "Incorrect kill game command usage. Please try again.\n");
        return;
    }
    sprintf(clientMessage, "KILLGAME 099292\n");
    sendUDPMessage(clientMessage, KILLGAME);
    printf("Game session killed...\n");
}

void clientKillDirectory(int numTokens)
{
    if (numTokens != 1)
    { // killdirectory
        fprintf(stderr, "Incorrect kill directory command usage. Please try again.\n");
        return;
    }
    sprintf(clientMessage, "KILLPDIR 099292\n");
    sendUDPMessage(clientMessage, KILLPDIR);
    printf("Directory killed...\n");
}