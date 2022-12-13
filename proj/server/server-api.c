#include "server-api.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

int currentWordFileLine = 0;

char *processServerStart(char **tokenList, int numTokens);
char *processServerPlay(char **tokenList, int numTokens);
// char *processServerGuess(char **tokenList, int numTokens);
// char *processServerQuit(char **tokenList, int numTokens);

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
    /*
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

char *getLineFromFile(char *filename, int lineNum)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        return NULL;
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int i = 0;
    while ((read = getline(&line, &len, file)) != -1)
    {
        if (i == lineNum)
        {
            fclose(file);
            line[strlen(line) - 1] = '\0';
            return line;
        }
        i++;
    }
    fclose(file);
    return NULL;
}

// getNumberOfLinesFromFile
int getNumberOfLinesFromFile(char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        return -1;
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int i = 0;
    while ((read = getline(&line, &len, file)) != -1)
    {
        i++;
    }
    fclose(file);
    return i;
}

// getGameWordFromFile: returns the first word from the first line of /games/PLID.txt
char *getGameWordFromFile(char *filename)
{
    // get first line from file
    char *line = getLineFromFile(filename, 0);
    if (line == NULL)
    {
        return NULL;
    }
    char *word = strtok(line, " ");
    return word;
}

// getGameHintFromFile: returns the second word from the first line of /games/PLID.txt
char *getGameHintFromFile(char *filename)
{
    // get first line from file
    char *line = getLineFromFile(filename, 0);
    if (line == NULL)
    {
        return NULL;
    }
    char *word = strtok(line, " ");
    char *hint = strtok(NULL, " ");
    return hint;
}

// getCorrectPlayPositions: returns string of all positions where the char played is correct
char *getCorrectPlayPositions(char *word, char *guess)
{
    char *positions = malloc(strlen(word) + 1);
    int i;
    int j = 0;
    for (i = 0; i < strlen(word); i++)
    {
        if (word[i] == guess[0])
        {
            positions[j] = i + '0';
            j++;
        }
    }
    positions[j] = '\0';
    return positions;
}

// hasBeenPlayedBefore: iterates through filename and checks if line has string already
int hasBeenPlayedBefore(char *filename, char *type, char *string)
{
    // type can be "play" or "guess"
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        return -1;
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, file)) != -1)
    {
        char *token1 = strtok(line, " ");
        char *token2 = strtok(NULL, " ");
        // if type == token1, check if token2 == string
        if (!strcmp(type, token1) && !strcmp(string, token2))
        {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
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

    // check if /games/PLID.txt exists
    char *filename = malloc(strlen(tokenList[1]) + 7);
    sprintf(filename, "games/%s.txt", tokenList[1]);
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
    printf("Created file %s.txt\n", tokenList[1]);
    char *line = getLineFromFile("word_eng.txt", currentWordFileLine);
    if (line == NULL)
    {
        free(filename);
        close(fd);
        return getServerReplyUDP(START, "NOK");
    }
    printf("Picked line with number: %d and content: %s\n", currentWordFileLine, line);
    // write the line to PLID.txt
    if (write(fd, line, strlen(line)) == -1)
    {
        free(filename);
        close(fd);
        return getServerReplyUDP(START, "NOK");
    }
    char *gameWord = strtok(line, " ");
    int gameWordLength = strlen(gameWord);
    int maxErrors = gameWordLength < 7 ? 7 : gameWordLength <= 10 ? 8
                                                                  : 9;
    currentWordFileLine = (currentWordFileLine + 1) % 25;
    char *serverReply = getServerReplyUDP(START, "OK");
    // add gameWordLength and maxErrors to serverReply
    char *finalServerReply = malloc(strlen(serverReply) + 10);
    // remove last char of serverReply
    serverReply[strlen(serverReply) - 1] = '\0';
    sprintf(finalServerReply, "%s %d %d\n", serverReply, gameWordLength, maxErrors);
    free(serverReply);
    free(line);
    free(filename);
    close(fd);
    return finalServerReply;
}

char *processServerPlay(char **tokenList, int numTokens)
{
    // tokenList = PLG PLID letter trial
    if (numTokens != 4)
    {
        return getServerReplyUDP(PLAY, "ERR");
    }
    else if (!isValidPLID(tokenList[1]))
    {
        return getServerReplyUDP(PLAY, "ERR");
    }
    else if (!isValidPlay(tokenList[2]))
    {
        return getServerReplyUDP(PLAY, "ERR");
    }
    else if (!isValidTrial(tokenList[3]))
    {
        return getServerReplyUDP(PLAY, "ERR");
    }
    // check if games/PLID.txt exists
    char *filename = malloc(strlen(tokenList[1]) + 7);
    sprintf(filename, "games/%s.txt", tokenList[1]);
    if (access(filename, F_OK) != -1)
    {
        free(filename);
        return getServerReplyUDP(START, "ERR");
    }
    // check if tokenList[3] is equal to the number of lines in games/PLID.txt
    int numLines = getNumberOfLinesFromFile(filename);
    if (numLines == -1)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR");
    }
    else if (numLines != atoi(tokenList[3]))
    {
        free(filename);
        return getServerReplyUDP(PLAY, "INV");
    }
    // get word from first line of games/PLID.txt
    char *gameWord = getGameWordFromFile(filename);
    if (gameWord == NULL)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR");
    }
    // check if has been played before
    if (hasBeenPlayedBefore(filename, "play", tokenList[2]))
    {
        free(filename);
        return getServerReplyUDP(PLAY, "DUP");
    }
    // get correct positions of tokenList[2] in gameWord
    char *correctPositions = getCorrectPlayPositions(gameWord, tokenList[2]);
    if (correctPositions == NULL)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "NOK");
    }

    free(filename);
    return getServerReplyUDP(PLAY, "OK");
}