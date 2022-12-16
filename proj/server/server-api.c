#include "server-api.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

int currentWordFileLine = 0;

int getNumberOfLinesFromFile(char *filename);
char *processServerStart(char **tokenList, int numTokens);
char *processServerPlay(char **tokenList, int numTokens);
char *processServerGuess(char **tokenList, int numTokens);
char *processServerQuit(char **tokenList, int numTokens);

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

static char *getServerReplyUDP(int command, char *status, int currentTrial)
{
    char message[SERVER_MESSAGE_UDP_SIZE] = {'\0'};
    switch (command)
    {
    case START:
        sprintf(message, "RSG %s\n", status);
        break;
    case PLAY:
        if (strcmp(status, "ERR"))
        {
            sprintf(message, "RLG %s %d\n", status, currentTrial);
        }
        else
        {
            sprintf(message, "RLG %s\n", status);
        }
        break;
    case GUESS:
        if (strcmp(status, "ERR"))
        {
            sprintf(message, "RWG %s %d\n", status, currentTrial);
        }
        else
        {
            sprintf(message, "RWG %s\n", status);
        }
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
    // get second word from line
    char *hint = strtok(line, " ");
    hint = strtok(NULL, " ");
    return hint;
}

// getCorrectPlayPositions: returns array of integers i of all indexes where guess = word[i]
int *getCorrectPlayPositions(char *word, char *guess)
{
    int *positions = malloc(sizeof(int) * strlen(word));
    int i = 0;
    int j = 0;
    while (word[i] != '\0')
    {
        if (word[i] == guess[0])
        {
            positions[j] = i + 1; // index + 1;
            j++;
        }
        i++;
    }
    positions[j] = -1;
    return positions;
}

// hasBeenPlayedBefore: iterates through filename and checks if string is a line inside file
int hasBeenPlayedBefore(char *filename, char *type, char *string)
{
    // concatenate [type] + [string] and check if it exists in file
    char *line = malloc(sizeof(char) * (strlen(type) + strlen(string) + 2));
    strcpy(line, type);
    strcat(line, " ");
    strcat(line, string);
    strcat(line, "\n");
    // type can be "play" or "guess"
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        free(line);
        return -1;
    }
    char *fileLine = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&fileLine, &len, file)) != -1)
    {
        if (!strcmp(fileLine, line))
        {
            fclose(file);
            free(line);
            return 1;
        }
    }
    fclose(file);
    free(line);
    return 0;
}

// writePlayToFile: writes a play to /games/PLID.txt
int writePlayToFile(char *filename, char *play, char *guess)
{
    FILE *file = fopen(filename, "a");
    if (file == NULL)
    {
        return -1;
    }
    fprintf(file, "%s %s\n", play, guess);
    fclose(file);
    return 0;
}

int getMaxErrors(int lengthWord)
{
    if (lengthWord < 7)
    {
        return 7;
    }
    else if (lengthWord <= 10)
    {
        return 8;
    }
    else
    {
        return 9;
    }
}

// getGameScore: returns the score of the game in /games/PLID.txt
int getGameScore(int errorsLeft, int wordLength)
{
    int maxErrors = getMaxErrors(wordLength);
    return (int)((float)errorsLeft / (float)maxErrors * 100);
}

int handleGameEnding(char *filename, char *PLID, int gameState, int gameScore)
{
    FILE *gameFile = fopen(filename, "a");
    if (gameFile == NULL)
    {
        return -1;
    }
    if (gameState == GAME_WON)
    {
        fprintf(gameFile, "CONGRATS you just won the game with a score of %d!\n", gameScore);
    }
    else
    {
        fprintf(gameFile, "GAME OVER.. better luck next time. Your score is 0.\n");
    }
    fclose(gameFile);
    // rename filename into scores/PLID.txt
    char *newFilename = malloc(sizeof(char) * (strlen(PLID) + 7));
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char date[20];
    sprintf(date, "%02d%02d%04d_%02d%02d%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    sprintf(newFilename, "scores/%d_%s_%s.txt", gameScore, PLID, date);
    rename(filename, newFilename);
    free(newFilename);
    return 0;
}

// getGameStateFromFile: returns [game_state, errorsLeft]
int *getGameStateFromFile(char *filename)
{
    int *result = malloc(sizeof(int) * 2);
    // get the word from the first line of the file
    char *word = getGameWordFromFile(filename);
    if (word == NULL)
    {
        result[0] = GAME_ERROR;
        result[1] = -1;
        return result;
    }
    int errorsLeft = getMaxErrors(strlen(word));
    int lettersLeft = strlen(word);
    int numLines = getNumberOfLinesFromFile(filename);
    for (int i = 1; i < numLines; i++)
    {
        char *line = getLineFromFile(filename, i);
        if (line == NULL)
        {
            result[0] = GAME_ERROR;
            result[1] = -1;
            return result;
        }
        // remove /n from the end of the line
        line[strlen(line) - 1] = '\0';
        char *token1 = strtok(line, " ");
        char *token2 = strtok(NULL, " ");
        // if token1 == "play", check if token2 is in word
        if (!strcmp(token1, "play"))
        {
            if (strchr(word, token2[0]) == NULL)
            {
                errorsLeft--;
                if (errorsLeft == 0)
                {
                    result[0] = GAME_LOST;
                    result[1] = 0;
                    return result;
                }
            }
            else
            {
                // letterLeft -= number of times token2[0] appears in word
                for (int j = 0; j < strlen(word); j++)
                {
                    if (word[j] == token2[0])
                        lettersLeft--;
                }
                if (lettersLeft == 0)
                {
                    result[0] = GAME_WON;
                    result[1] = errorsLeft;
                    return result;
                }
            }
        }
        else if (!strcmp(token1, "guess"))
        {
            if (!strcmp(token2, word))
            {
                result[0] = GAME_WON;
                result[1] = errorsLeft;
                return result;
            }
            else
            {
                errorsLeft--;
                if (errorsLeft == 0)
                {
                    result[0] = GAME_LOST;
                    result[1] = 0;
                    return result;
                }
            }
        }
        free(line);
    }
    result[0] = GAME_CONTINUE;
    result[1] = errorsLeft;
    return result;
}

char *processServerStart(char **tokenList, int numTokens)
{
    if (numTokens != 2)
    {
        return getServerReplyUDP(START, "ERR", 0);
    }
    else if (!isValidPLID(tokenList[1]))
    {
        return getServerReplyUDP(START, "ERR", 0);
    }

    // check if /games/PLID.txt exists
    char *filename = malloc(strlen(tokenList[1]) + 7);
    sprintf(filename, "games/%s.txt", tokenList[1]);
    if (access(filename, F_OK) != -1)
    {
        free(filename);
        printf("Error: Player with PLID %s already has an ongoing game.\n", tokenList[1]);
        return getServerReplyUDP(START, "NOK", 0);
    }
    // create the file PLID.txt
    int fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
    {
        free(filename);
        return getServerReplyUDP(START, "ERR", 0);
    }
    printf("Created file %s.txt\n", tokenList[1]);
    char *line = getLineFromFile("word_eng.txt", currentWordFileLine);
    if (line == NULL)
    {
        free(filename);
        close(fd);
        return getServerReplyUDP(START, "ERR", 0);
    }
    printf("Picked line with number: %d and content: %s", currentWordFileLine, line);
    // write the line to PLID.txt
    if (write(fd, line, strlen(line)) == -1)
    {
        free(filename);
        close(fd);
        return getServerReplyUDP(START, "ERR", 0);
    }
    char *gameWord = strtok(line, " ");
    int gameWordLength = strlen(gameWord);
    int maxErrors = getMaxErrors(gameWordLength);
    currentWordFileLine = (currentWordFileLine + 1) % 25;
    char *serverReply = getServerReplyUDP(START, "OK", 0);
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
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    else if (!isValidPLID(tokenList[1]))
    {
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    else if (!isValidPlay(tokenList[2]))
    {
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    else if (!isValidTrial(tokenList[3]))
    {
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    // check if PLID.txt doesnt exist in games folder
    char *filename = malloc(strlen(tokenList[1]) + 7);
    sprintf(filename, "games/%s.txt", tokenList[1]);
    if (access(filename, F_OK) == -1)
    {
        printf("Error, file %s doesn't exist.\n", filename);
        free(filename);
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    // check if tokenList[3] (trial sent by player) is equal to the currentTrial
    int currentTrial = getNumberOfLinesFromFile(filename);
    if (currentTrial == -1)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    else if (currentTrial != atoi(tokenList[3]))
    {
        free(filename);
        return getServerReplyUDP(PLAY, "INV", currentTrial);
    }
    // get word from first line of games/PLID.txt
    char *gameWord = getGameWordFromFile(filename);
    if (gameWord == NULL)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    // check if has been played before
    if (hasBeenPlayedBefore(filename, "play", tokenList[2]) == 1)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "DUP", currentTrial);
    }
    // write play to games/PLID.txt
    if (writePlayToFile(filename, "play", tokenList[2]) == -1)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    // get game state and errorsLeft from getGameStateFromFile
    int *gameState = getGameStateFromFile(filename);
    if (gameState == NULL)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    int gameStateResult = gameState[0];
    int errorsLeft = gameState[1];
    if (gameStateResult == GAME_ERROR)
    {
        free(filename);
        return getServerReplyUDP(PLAY, "ERR", 0);
    }
    else if (gameStateResult == GAME_WON)
    {
        int gameScore = getGameScore(errorsLeft, strlen(gameWord));
        handleGameEnding(filename, tokenList[1], GAME_WON, gameScore);
        free(filename);
        return getServerReplyUDP(PLAY, "WIN", currentTrial);
    }
    else if (gameStateResult == GAME_LOST)
    {
        handleGameEnding(filename, tokenList[1], GAME_LOST, 0);
        free(filename);
        return getServerReplyUDP(PLAY, "OVR", currentTrial);
    }
    int *correctPositions = getCorrectPlayPositions(gameWord, tokenList[2]);
    // if correctPositions if empty then the user didn't guess any letter
    if (correctPositions[0] == -1)
    {
        free(filename);
        free(correctPositions);
        return getServerReplyUDP(PLAY, "NOK", currentTrial);
    }
    // server reply = "RLG OK trial\n"
    char *serverReply = getServerReplyUDP(PLAY, "OK", currentTrial);
    // remove last char of serverReply
    serverReply[strlen(serverReply) - 1] = '\0';
    int numCorrectPositions = 0;
    while (correctPositions[numCorrectPositions] != -1)
    {
        numCorrectPositions++;
    }
    // finalServerReply = "RLG OK trial numCorrectPositions correctPositions*(divided by spaced)
    char *finalServerReply = malloc(strlen(serverReply) + 10);
    sprintf(finalServerReply, "%s %d", serverReply, numCorrectPositions);
    free(serverReply);
    // add correctPositions separated by spaces to finalServerReply
    for (int i = 0; i < numCorrectPositions; i++)
    {
        strcat(finalServerReply, " ");
        char *position = malloc(2);
        sprintf(position, "%d", correctPositions[i]);
        strcat(finalServerReply, position);
        free(position);
    }
    strcat(finalServerReply, "\n");
    free(filename);
    free(correctPositions);
    return strdup(finalServerReply);
}

char *processServerGuess(char **tokenList, int numTokens)
{
    // tokenList = PWG PLID guess trial
    if (numTokens != 4)
    {
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    else if (!isValidPLID(tokenList[1]))
    {
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    else if (!isValidGuess(tokenList[2]))
    {
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    else if (!isValidTrial(tokenList[3]))
    {
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    // check if PLID.txt doesnt exist in games folder
    char *filename = malloc(strlen(tokenList[1]) + 7);
    sprintf(filename, "games/%s.txt", tokenList[1]);
    if (access(filename, F_OK) == -1)
    {
        printf("Error, file %s doesn't exist.\n", filename);
        free(filename);
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    // check if tokenList[3] is equal to the number of lines in games/PLID.txt
    int currentTrial = getNumberOfLinesFromFile(filename);
    if (currentTrial == -1)
    {
        free(filename);
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    else if (currentTrial != atoi(tokenList[3]))
    {
        free(filename);
        return getServerReplyUDP(GUESS, "INV", currentTrial);
    }
    // get word from first line of games/PLID.txt
    char *gameWord = getGameWordFromFile(filename);
    if (gameWord == NULL)
    {
        free(filename);
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    // write play to games/PLID.txt
    if (writePlayToFile(filename, "guess", tokenList[2]) == -1)
    {
        free(filename);
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    // get game state and errorsLeft from getGameStateFromFile
    int *gameState = getGameStateFromFile(filename);
    if (gameState == NULL)
    {
        free(filename);
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    int gameStateResult = gameState[0];
    int errorsLeft = gameState[1];
    if (gameStateResult == GAME_ERROR)
    {
        free(filename);
        return getServerReplyUDP(GUESS, "ERR", 0);
    }
    else if (gameStateResult == GAME_WON)
    {
        int gameScore = getGameScore(errorsLeft, strlen(gameWord));
        handleGameEnding(filename, tokenList[1], GAME_WON, gameScore);
        free(filename);
        return getServerReplyUDP(GUESS, "WIN", currentTrial);
    }
    else if (gameStateResult == GAME_LOST)
    {
        handleGameEnding(filename, tokenList[1], GAME_LOST, 0);
        free(filename);
        return getServerReplyUDP(GUESS, "OVR", currentTrial);
    }
    else
    {
        // gameStateResult == GAME_CONTINUE
        free(filename);
        return getServerReplyUDP(GUESS, "NOK", currentTrial);
    }
}

char *processServerQuit(char **tokenList, int numTokens)
{
    // tokenList = QUT PLID
    if (numTokens != 2)
    {
        return getServerReplyUDP(QUIT, "ERR", 0);
    }
    else if (!isValidPLID(tokenList[1]))
    {
        return getServerReplyUDP(QUIT, "ERR", 0);
    }
    // check if PLID.txt doesnt exist in games folder
    char *filename = malloc(strlen(tokenList[1]) + 7);
    sprintf(filename, "games/%s.txt", tokenList[1]);
    if (access(filename, F_OK) == -1)
    {
        printf("Error, file %s doesn't exist.\n", filename);
        free(filename);
        return getServerReplyUDP(QUIT, "NOK", 0);
    }
    // delete PLID.txt from games folder
    if (remove(filename) == -1)
    {
        printf("Error, couldn't delete file %s.\n", filename);
        free(filename);
        return getServerReplyUDP(QUIT, "ERR", 0);
    }
    free(filename);
    return getServerReplyUDP(QUIT, "OK", 0);
}
