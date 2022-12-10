#include "server-api.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

char *processServerStart(char **tokenList, int numTokens);

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
    /*
    case PLAY:
        response = processServerPlay(tokenList, numTokens);
        break;
    case GUESS:
        response = processServerGuess(tokenList, numTokens);
        break;
    case QUIT:
        response = processServerQuit(tokenList, numTokens);
        break;
    */
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
        /*
    case SCOREBOARD:
        processServerScoreboard(fd);
        break;
    case HINT:
        processServerHint(fd);
        break;
    case STATE:
        processServerState(fd);
        break;
        */
    default:
        if (sendTCPMessage(fd, ERROR_MSG) == -1)
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
    if (sendTCPMessage(fd, message) == -1)
    {
        close(fd);
        exit(EXIT_FAILURE);
    }
}

int getNumLines(int fd)
{
    int numLines = 0;
    char c;
    while (read(fd, &c, 1) == 1)
    {
        if (c == '\n')
        {
            numLines++;
        }
    }
    return numLines;
}

char *getLine(int fd, int lineNum)
{
    int numLines = 0;
    char c;
    char *line = NULL;
    int lineSize = 0;
    while (read(fd, &c, 1) == 1)
    {
        if (c == '\n')
        {
            numLines++;
            if (numLines == lineNum)
            {
                return line;
            }
            free(line);
            line = NULL;
            lineSize = 0;
        }
        else
        {
            line = realloc(line, lineSize + 1);
            line[lineSize++] = c;
        }
    }
    return NULL;
}

char *getLineFromFile(char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        return NULL;
    }
    int numLines = getNumLines(fd);
    int lineNum = rand() % numLines;
    char *line = getLine(fd, lineNum);
    close(fd);
    return line;
}

char *processServerStart(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    {
        return getServerReplyUDP(START, "NOK");
    }
    else if (!isValidPLID(tokenList[1]))
    {
        return getServerReplyUDP(START, "NOK");
    }

    // check if PLID.txt exists
    char *filename = malloc(strlen(tokenList[1]) + 5);
    sprintf(filename, "%s.txt", tokenList[1]);
    if (access(filename, F_OK) != -1)
    {
        free(filename);
        return getServerReplyUDP(START, "NOK");
    }
    // create the file PLID.txt
    int fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
    {
        free(filename);
        return getServerReplyUDP(START, "NOK");
    }
    // get one line from word_eng.txt
    char *line = getLineFromFile("word_eng.txt");
    if (line == NULL)
    {
        free(filename);
        close(fd);
        return getServerReplyUDP(START, "NOK");
    }
    // write the line to PLID.txt
    if (write(fd, line, strlen(line)) == -1)
    {
        free(filename);
        free(line);
        close(fd);
        return getServerReplyUDP(START, "NOK");
    }
    return getServerReplyUDP(START, "OK");
}
