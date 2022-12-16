#include "client-api.h"
#include "../constants.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* DS Server information variables */
char GSIP[GS_IP_SIZE] = GS_DEFAULT_IP;
char GSport[GS_PORT_SIZE] = GS_DEFAULT_PORT;

/* UDP Socket related variables */
int fdUDP;
struct addrinfo hintsUDP, *resUDP;
struct sockaddr_in addrUDP;
socklen_t addrlenUDP;

/* TCP Socket related variables */
int fdTCP;
struct addrinfo hintsTCP, *resTCP;
int TCPConnection = OFF;

/* Client current session variables */
int clientSession = LOGGED_OUT;
char clientPLID[CLIENT_PLID_SIZE];

/* Message to DS via UDP protocol variable */
char clientMessage[CLIENT_MESSAGE_UDP_SIZE] = {'\0'};

char currentWord[MAX_WORD_LENGTH_SIZE] = {'\0'};

char currentWordWithSpaces[MAX_WORD_LENGTH_SIZE] = {'\0'};

char guessedWord[MAX_WORD_LENGTH_SIZE] = {'\0'};

int wordLength;

int maxNumberErrors;

int currentTrial = 1;

int currentErrors = 0;

char letterGuess[1];

static void failUDP();

static void errUDP();

static void failTCP();

static void errTCP();

// Returns currentWord with spaces between them
char *getCurrentWordWithSpaces()
{
    int i;
    for (i = 0; i < wordLength; i++)
    {
        currentWordWithSpaces[i * 2] = currentWord[i];
        currentWordWithSpaces[i * 2 + 1] = ' ';
    }
    currentWordWithSpaces[wordLength * 2] = '\0';
    return currentWordWithSpaces;
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
        // dont wait for reply from GS if command == KILLGAME or KILLPDIR or Exit
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
    printf("Server response: %s %s\n", serverCommand, serverStatus);
    if (!strcmp(serverCommand, "RSG"))
    { // START response
        if (!strcmp(serverStatus, "OK"))
        {
            clientSession = LOGGED_IN;
            sscanf(message, "%*s %*s %d %d", &wordLength, &maxNumberErrors);
            for (int i = 0; i < wordLength; i++)
            {
                currentWord[i] = '_';
            }
            currentWord[wordLength] = '\0';
            printf("New game started (max %d errors): %s\n", maxNumberErrors, getCurrentWordWithSpaces());
        }
        else if (!strcmp(serverStatus, "NOK"))
        {
            clientSession = LOGGED_IN;
            fprintf(stderr, "You already have a playing session in progress. To play use <play> command.\n");
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
            // get all positions and store currentLetter in those positions of guessedWord
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
            // iterate through guessedWord and if the letter is '_' then change it to letterGuess
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
            currentErrors++;
            printf("No, '%c' is not part of the word: %s [%d Errors left]\n", letterGuess[0], getCurrentWordWithSpaces(), maxNumberErrors - currentErrors);
            currentTrial++;
        }
        else if (!strcmp(serverStatus, "OVR"))
        {
            printf("No, '%c' is not part of the word: %s\n", letterGuess[0], getCurrentWordWithSpaces());
            printf("GAME OVER! You lost because you reached the maximum failed attempts of %d. Better luck next time!\n", maxNumberErrors);
            clientSession = LOGGED_OUT;
            memset(clientPLID, 0, sizeof(clientPLID));
        }
        else if (!strcmp(serverStatus, "INV"))
        {
            printf("The trial number that you entered is not valid or you have already played a different letter for that trial. Please try again.\n");
        }
        else if (!strcmp(serverStatus, "ERR"))
        {
            printf("PLID is invalid or there is no ongoing game for that specific PLID. Please try again.\n");
        }
        else
        {
            errUDP();
        }
    }
    else if (!strcmp(serverCommand, "RWG"))
    { // Guess response
        if (!strcmp(serverStatus, "WIN"))
        {
            // iterate through guessedWord and if the letter is '_' then change it to letterGuess
            for (int i = 0; i < wordLength; i++)
            {
                if (currentWord[i] == '_')
                {
                    currentWord[i] = letterGuess[0];
                }
            }
            // guessedWordWithSpaces = guessedWord with spaces in between
            char guessedWordWithSpaces[strlen(guessedWord) * 2];
            for (int i = 0; i < strlen(guessedWord); i++)
            {
                guessedWordWithSpaces[i * 2] = guessedWord[i];
                guessedWordWithSpaces[i * 2 + 1] = ' ';
            }
            guessedWordWithSpaces[strlen(guessedWord) * 2] = '\0';
            printf("WELL DONE! You guessed: %s\n", guessedWordWithSpaces);
            clientSession = LOGGED_OUT;
            memset(clientPLID, 0, sizeof(clientPLID));
        }
        else if (!strcmp(serverStatus, "NOK"))
        {
            currentErrors++;
            printf("No, '%s' is not the correct word: %s [%d Errors left]\n", guessedWord, getCurrentWordWithSpaces(), maxNumberErrors - currentErrors);
            currentTrial++;
        }
        else if (!strcmp(serverStatus, "OVR"))
        {
            printf("No, '%s' is not the correct word: %s\n", guessedWord, getCurrentWordWithSpaces());
            printf("GAME OVER! You lost because you reached the maximum failed attempts of %d. Better luck next time!\n", maxNumberErrors);
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
    else if (!strcmp(serverCommand, "RQT"))
    { // Quit/Exit response
        if (!strcmp(serverStatus, "OK"))
        {
            clientSession = LOGGED_OUT;
            memset(clientPLID, 0, sizeof(clientPLID));
            printf("You quit the game successfully.\n");
        }
        else if (!strcmp(serverStatus, "ERR"))
        {
            printf("You currently don't have an open gaming session. Please use <start> command to start a new session.\n");
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
        perror("Failed to translate TCP address");
        close(fdTCP);
        failUDP();
    }
    int n = connect(fdTCP, resTCP->ai_addr, resTCP->ai_addrlen);
    if (n == -1)
    {
        perror("Failed to connect to TCP socket");
        failTCP();
    }
    TCPConnection = ON;
}

void processTCPReply()
{
    // Read message from Server
    int lenMsg = TCP_MESSAGE_READ_BUFFER_SIZE;
    char *tmp = (char *)calloc(sizeof(char), lenMsg + 1);
    if (tmp == NULL)
    {
        fprintf(stderr, "Failed to allocate memory in calloc.\n");
        closeTCPSocket(fdTCP, resTCP);
        TCPConnection = OFF;
        return;
    }
    char *p_message = tmp;
    char readBuffer[TCP_MESSAGE_READ_BUFFER_SIZE];
    int n, bytesRead = 0;
    // read all data from server
    while ((n = read(fdTCP, readBuffer, TCP_MESSAGE_READ_BUFFER_SIZE)) > 0)
    {
        bytesRead += n;
        if (bytesRead > lenMsg)
        {
            lenMsg += TCP_MESSAGE_READ_BUFFER_SIZE;
            tmp = (char *)realloc(tmp, lenMsg + 1);
            if (tmp == NULL)
            {
                fprintf(stderr, "Failed to allocate memory in realloc.\n");
                closeTCPSocket(fdTCP, resTCP);
                TCPConnection = OFF;
                return;
            }
            p_message = tmp + bytesRead - n;
        }
        memcpy(p_message, readBuffer, n);
        p_message += n;
        if (readBuffer[n - 1] == '\0')
        {
            break;
        }
    }
    if (n == -1)
    {
        perror("Failed to read from TCP socket");
        closeTCPSocket(fdTCP, resTCP);
        TCPConnection = OFF;
        return;
    }
    char *token = strtok(tmp, " ");
    char *serverCommand = token;
    token = strtok(NULL, " ");
    char *serverStatus = token;
    if (!strcmp(serverCommand, "RSB"))
    // Process message (RSB status [Fname Fsize Fdata])
    {
        if (!strcmp(serverStatus, "OK"))
        {
            token = strtok(NULL, " ");
            char *fileName = token;
            token = strtok(NULL, " ");
            char *fileSize = token;
            int fileSizeInt = atoi(fileSize);
            char *fileData = token + strlen(fileSize) + 1;
            fileData[fileSizeInt] = '\0';
            // Save file in current directory
            FILE *fp;
            fp = fopen(fileName, "w");
            if (fp == NULL)
            {
                perror("Failed to open file");
                closeTCPSocket(fdTCP, resTCP);
                TCPConnection = OFF;
                return;
            }
            fwrite(fileData, 1, fileSizeInt, fp);
            fclose(fp);
            printf("Scoreboard received successfuly! File name: %s File size: %d bytes.\n", fileName, fileSizeInt);
            printf("%s", fileData);
        }
        else if (!strcmp(serverStatus, "ERR"))
        {
            printf("No scoreboard file was found on the server.\n");
        }
        else
        {
            errTCP();
        }
    }
    else if (!strcmp(serverCommand, "RHL"))
    // Process message (RHL status [Fname Fsize Fdata])
    {
        if (!strcmp(serverStatus, "OK"))
        {
            token = strtok(NULL, " ");
            char *fileName = token;
            token = strtok(NULL, " ");
            char *fileSize = token;
            int fileSizeInt = atoi(fileSize);
            char *fileData = token + strlen(fileSize) + 1;
            fileData[fileSizeInt] = '\0';
            // Save file in current directory
            FILE *fp;
            fp = fopen(fileName, "w");
            if (fp == NULL)
            {
                perror("Failed to open file");
                closeTCPSocket(fdTCP, resTCP);
                TCPConnection = OFF;
                return;
            }
            fwrite(fileData, 1, fileSizeInt, fp);
            fclose(fp);
            printf("Hint received successfuly! File name: %s File size: %d bytes.\n", fileName, fileSizeInt);
        }
        else if (!strcmp(serverStatus, "NOK"))
        {
            printf("There is no hint available at the moment. Please try again later.\n");
        }
        else if (!strcmp(serverStatus, "ERR"))
        {
            printf("Some problem occured. Please try again.\n");
        }
        else
        {
            errTCP();
        }
    }
    else if (!strcmp(serverCommand, "RST"))
    // Process message (RST status [Fname Fsize Fdata])
    {
        if (!strcmp(serverStatus, "ACT"))
        {
            token = strtok(NULL, " ");
            char *fileName = token;
            token = strtok(NULL, " ");
            char *fileSize = token;
            int fileSizeInt = atoi(fileSize);
            char *fileData = token + strlen(fileSize) + 1;
            fileData[fileSizeInt] = '\0';
            // Save file in current directory
            FILE *fp;
            fp = fopen(fileName, "w");
            if (fp == NULL)
            {
                perror("Failed to open file");
                closeTCPSocket(fdTCP, resTCP);
                TCPConnection = OFF;
                return;
            }
            fclose(fp);
            printf("State received successfuly! File name: %s File size: %d bytes.\n", fileName, fileSizeInt);
            printf("%s", fileData);
        }
        else if (!strcmp(serverStatus, "FIN"))
        {
            token = strtok(NULL, " ");
            char *fileName = token;
            token = strtok(NULL, " ");
            char *fileSize = token;
            int fileSizeInt = atoi(fileSize);
            char *fileData = token + strlen(fileSize) + 1;
            fileData[fileSizeInt] = '\0';
            // Save file in current directory
            FILE *fp;
            fp = fopen(fileName, "w");
            if (fp == NULL)
            {
                perror("Failed to open file");
                closeTCPSocket(fdTCP, resTCP);
                TCPConnection = OFF;
                return;
            }
            fclose(fp);
            printf("State received successfuly! File name: %s File size: %d bytes.\n", fileName, fileSizeInt);
            printf("%s", fileData);
        }
        else if (!strcmp(serverStatus, "NOK"))
        {
            printf("You don't have any active or finished games. To play use command <start> and try again.\n");
        }
        else if (!strcmp(serverStatus, "ERR"))
        {
            printf("Some problem occured. Please try again.\n");
        }
        else
        {
            errTCP();
        }
    }
    else
    {
        errTCP();
    }
    free(tmp);
    closeTCPSocket(fdTCP, resTCP);
    TCPConnection = OFF;
}

static void failTCP()
{
    closeUDPSocket(fdUDP, resUDP);
    closeTCPSocket(fdTCP, resTCP);
    TCPConnection = OFF;
    exit(EXIT_FAILURE);
}

static void errTCP()
{
    fprintf(stderr, "Wrong protocol message received from server via TCP. Program will now exit.\n");
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
        fprintf(stderr, "Incorrect start command usage. Please try again.\n");
        return;
    }
    if (!isValidPLID(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "Invalid start command arguments. Please check given PLID and try again.\n");
        return;
    }
    if (clientSession == LOGGED_IN)
    {
        fprintf(stderr, "You already have a playing session undergoing. Please use command <play> and try again.\n");
        return;
    }
    // SNG PLID
    currentTrial = 1;
    strcpy(clientPLID, tokenList[1]);
    sprintf(clientMessage, "SNG %s\n", tokenList[1]);
    sendUDPMessage(clientMessage, START);
}

void clientPlay(char **tokenList, int numTokens)
{
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
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You don't have an ongoing playing session. Please use <start> command and use <start> command and try again.\n");
        return;
    }
    // GUESS PLID word trial
    strcpy(guessedWord, tokenList[1]);
    sprintf(clientMessage, "PWG %s %s %d\n", clientPLID, tokenList[1], currentTrial);
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
    processTCPReply();
}

void clientHint(int numTokens)
{

    if (numTokens != 1)
    { // hint/h
        fprintf(stderr, "Incorrect hint command usage. Please try again.\n");
        return;
    }
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You don't have an ongoing playing session. Please use <start> command and try again.\n");
        return;
    }
    connectTCPSocket();
    sprintf(clientMessage, "GHL %s\n", clientPLID);
    if (sendTCPMessage(fdTCP, clientMessage) == -1)
    {
        failTCP();
    }
    processTCPReply();
}

void clientState(int numTokens)
{
    if (numTokens != 1)
    { // state/st
        fprintf(stderr, "Incorrect state command usage. Please try again.\n");
        return;
    }
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You don't have an ongoing playing session. Please use <start> command and try again.\n");
        return;
    }
    connectTCPSocket();
    sprintf(clientMessage, "STA %s\n", clientPLID);
    if (sendTCPMessage(fdTCP, clientMessage) == -1)
    {
        failTCP();
    }
    processTCPReply();
}

void clientQuit(int numTokens)
{
    if (numTokens != 1)
    { // quit
        fprintf(stderr, "Incorrect quit command usage. Please try again.\n");
        return;
    }
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        fprintf(stderr, "You don't have an ongoing playing session. Please use <start> command and try again.\n");
        return;
    }
    printf("Quitting...\n");
    sprintf(clientMessage, "QUT %s\n", clientPLID);
    sendUDPMessage(clientMessage, QUIT);
    if (TCPConnection == ON)
    {
        closeTCPSocket(fdTCP, resTCP);
        TCPConnection = OFF;
    }
}

void clientExit(int numTokens)
{
    if (numTokens != 1)
    { // exit
        fprintf(stderr, "Incorrect exit command usage. Please try again.\n");
        return;
    }
    if (clientSession == LOGGED_OUT)
    { // Client does not have a playing session
        printf("Exiting...\n");
        closeUDPSocket(fdUDP, resUDP);
        exit(EXIT_SUCCESS);
    }
    printf("Exiting...\n");
    sprintf(clientMessage, "QUT %s\n", clientPLID);
    sendUDPMessage(clientMessage, EXIT);
    if (TCPConnection == ON)
    {
        closeTCPSocket(fdTCP, resTCP);
        TCPConnection = OFF;
    }
    closeUDPSocket(fdUDP, resUDP);
    exit(EXIT_SUCCESS);
}

void clientKillGame(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    { // killgame PLID
        fprintf(stderr, "Incorrect kill game command usage. Please try again.\n");
        return;
    }
    if (!isValidPLID(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "Invalid start killgame arguments. Please check given PLID and try again.\n");
        return;
    }
    sprintf(clientMessage, "KILLGAME %s\n", tokenList[1]);
    sendUDPMessage(clientMessage, KILLGAME);
    printf("Game session killed...\n");
}

void clientKillDirectory(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    { // killdirectory PLID
        fprintf(stderr, "Incorrect kill directory command usage. Please try again.\n");
        return;
    }
    if (!isValidPLID(tokenList[1]))
    { // Protocol validation
        fprintf(stderr, "Invalid killdirectory arguments. Please check given PLID and try again.\n");
        return;
    }
    sprintf(clientMessage, "KILLPDIR %s\n", tokenList[1]);
    sendUDPMessage(clientMessage, KILLPDIR);
    printf("Directory killed...\n");
}