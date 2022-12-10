#include "server-api.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

char *processClientUDP(char *message)
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
    cmd = parseDSClientCommand(tokenList[0]);
    switch (cmd)
    {
    case REGISTER:
        response = clientRegister(tokenList, numTokens);
        break;
    case UNREGISTER:
        response = clientUnregister(tokenList, numTokens);
        break;
    case LOGIN:
        response = clientLogin(tokenList, numTokens);
        break;
    case LOGOUT:
        response = clientLogout(tokenList, numTokens);
        break;
    case GROUPS:
        response = listDSGroups(numTokens);
        break;
    case SUBSCRIBE:
        response = clientSubscribeGroup(tokenList, numTokens);
        break;
    case UNSUBSCRIBE:
        response = clientUnsubscribeGroup(tokenList, numTokens);
        break;
    case MY_GROUPS:
        response = listClientDSGroups(tokenList, numTokens);
        break;
    default:
        response = strdup(ERROR_MSG);
        break;
    }
    return response;
}

void processClientTCP(int fd, char *command)
{
    int cmd = parseDSClientCommand(command);
    switch (cmd)
    {
    case ULIST:
        showClientsInGroup(fd);
        break;
    case POST:
        clientPostInGroup(fd);
        break;
    case RETRIEVE:
        retrieveMessagesFromGroup(fd);
        break;
    default:
        if (sendTCP(fd, ERROR_MSG) == -1)
        {
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
}

static char *createServerReplyUDP(int command, char *status)
{
    char message[SERVER_MESSAGE_UDP_SIZE];
    switch (command)
    {
    case REGISTER:
        sprintf(message, "RRG %s\n", status);
        break;
    case UNREGISTER:
        sprintf(message, "RUN %s\n", status);
        break;
    case LOGIN:
        sprintf(message, "RLO %s\n", status);
        break;
    case LOGOUT:
        sprintf(message, "ROU %s\n", status);
        break;
    case GROUPS:
        sprintf(message, "RGL %s\n", status);
        break;
    case SUBSCRIBE:
        sprintf(message, "RGS %s\n", status);
        break;
    case UNSUBSCRIBE:
        sprintf(message, "RGU %s\n", status);
        break;
    case MY_GROUPS:
        sprintf(message, "RGM %s\n", status);
        break;
    }
    return strdup(message);
}

static void sendServerStatusTCP(int fd, int command, char *status)
{
    char message[SERVER_TCP_STATUS_SIZE];
    switch (command)
    {
    case ULIST:
        if (!strcmp(status, "NOK"))
        {
            sprintf(message, "RUL NOK\n");
        }
        else
        {
            sprintf(message, "RUL %s", status);
        }
        break;
    case POST:
        sprintf(message, "RPT %s\n", status);
        break;
    case RETRIEVE:
        if (!strcmp(status, "NOK") || !strcmp(status, "EOF"))
        {
            sprintf(message, "RRT %s\n", status);
        }
        else
        {
            sprintf(message, "RRT %s", status);
        }
    }
    if (sendTCP(fd, message) == -1)
    {
        close(fd);
        exit(EXIT_FAILURE);
    }
}
