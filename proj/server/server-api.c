#include "server-api.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

char *processServerUDP(char *message)
{
    char *token, *tokenList[CLIENT_NUMTOKENS];
    int numTokens = 0;
    int cmd;
    char *response;
    token = strtok(message, " ");
    while (token)
    {
        tokenList[numTokens++] = token;
        token = strtok(NULL, " ");
    }
    cmd = parseServerCommand(tokenList[0]);
    switch (cmd)
    {
    case START:
        response = processServerStart(tokenList, numTokens);
        break;
    case PLAY:
        response = processServerPlay(tokenList, numTokens);
        break;
    case GUESS:
        response = processServerGuess(tokenList, numTokens);
        break;
    case QUIT:
        response = processServerQuit(tokenList, numTokens);
        break;
    default:
        response = strdup(ERROR_MSG);
        break;
    }
    return response;
}

void processServerTCP(int fd, char *command)
{
    int cmd = parseServerCommand(command);
    switch (cmd)
    {
    case SCOREBOARD:
        processServerScoreboard(fd);
        break;
    case HINT:
        processServerHint(fd);
        break;
    case STATE:
        processServerState(fd);
        break;
    default:
        if (sendTCP(fd, ERROR_MSG) == -1)
        {
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
}

static char *getServerReplyUDP(int command, char *status)
{
    char message[SERVER_MESSAGE_UDP_SIZE];
    switch (command)
    {
    case START:
        sprintf(message, "RSG %s\n", status);
        break;
    case PLAY:
        sprintf(message, "RLG %s\n", status);
        break;
    case GUESS:
        sprintf(message, "RWG %s\n", status);
        break;
    case QUIT:
        sprintf(message, "RQT %s\n", status);
        break;
    }
    return strdup(message);
}

static void sendServerStatusTCP(int fd, int command, char *status)
{
    char message[SERVER_TCP_STATUS_SIZE];
    switch (command)
    {
    case SCOREBOARD:
        if (!strcmp(status, "NOK"))
        {
            sprintf(message, "RUL NOK\n");
        }
        else
        {
            sprintf(message, "RSB %s", status);
        }
        break;
    case HINT:
        sprintf(message, "RHL %s\n", status);
        break;
    case STATE:
        if (!strcmp(status, "NOK") || !strcmp(status, "EOF"))
        {
            sprintf(message, "RRT %s\n", status);
        }
        else
        {
            sprintf(message, "RST %s", status);
        }
    }
    if (sendTCP(fd, message) == -1)
    {
        close(fd);
        exit(EXIT_FAILURE);
    }
}
