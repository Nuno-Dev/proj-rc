#include "../hangman-api.h"
#include "../hangman-api-constants.h"
#include "client-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parse arguments */
static void parseArgs(int argc, char *argv[])
{
    if (!(argc == 1 || argc == 3 || argc == 5))
    { // Usage: ./player [-n GSIP] [-p GSport]
        fprintf(stderr, "[-] Invalid client program arguments. Usage: ./player [-n GSIP] [-p GSport]\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-') // if not flag ignore
            continue;

        switch (argv[i][1])
        {
        case 'n':
            if (isValidAddress(argv[i + 1]))
                strcpy(GSIP, argv[i + 1]);
            else
            {
                fprintf(stderr, "[-] Invalid GS hostname/IP address given. Please try again.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            if (isValidPort(argv[i + 1]))
                strcpy(GSport, argv[i + 1]);
            else
            {
                fprintf(stderr, "[-] Invalid GS port given. Please try again.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "[-] Invalid flag given. Usage: ./player [-n GSIP] [-p GSport]\n");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * @brief Process all comands given by the client.
 */

void processInput()
{
    int command_number;
    while (1)
    {
        char command[CLIENT_COMMAND_SIZE];
        char *tokenList[CLIENT_NUMTOKENS];
        char *token;
        int numTokens = 0;
        printf(">>> "); // For user input
        fgets(command, sizeof(command), stdin);
        strtok(command, "\n");
        char commandTok[CLIENT_COMMAND_SIZE]; // We must preserve command so perform token separation here
        strcpy(commandTok, command);
        token = strtok(commandTok, " ");
        if (token[0] == '\n') // user presses enter without input ro
            continue;
        while (token)
        {
            tokenList[numTokens++] = token;
            token = strtok(NULL, " ");
        }
        command_number = parseClientCommand(tokenList[0]);
        switch (command_number)
        {
        case START:
            clientStart(tokenList, numTokens);
            break;
        case PLAY:
            clientPlay(tokenList, numTokens);
            break;
        case GUESS:
            clientGuess(tokenList, numTokens);
            break;
        case SCOREBOARD:
            clientScoreboard(numTokens);
            break;
        case HINT:
            clientHint(numTokens);
            break;
        case STATE:
            clientState(numTokens);
            break;
        case QUIT:
            clientQuit(numTokens);
            break;
        case EXIT:
            clientExit(numTokens);
            break;
        case KILLGAME:
            clientKillGame(tokenList, numTokens);
            break;
        case KILLPDIR:
            clientKillDirectory(tokenList, numTokens);
            break;
        default:
            break;
        }
    }
}

/**
 * @brief Main client program
 */

int main(int argc, char *argv[])
{
    parseArgs(argc, argv);
    createUDPSocket();
    processInput();
    exit(EXIT_SUCCESS);
}
