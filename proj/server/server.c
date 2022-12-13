#include "server-api.h"
#include "server-connection.h"
#include "../common.h"
#include "../constants.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static void parseArgs(int argc, char *argv[])
{ // Usage: ./GS word_eng.txt [-p GSport] [-v]
    if (!(argc == 2 || argc == 3 || argc == 4 || argc == 5))
    {
        fprintf(stderr, "Invalid server program arguments. Usage: ./GS word_file [-p GSport] [-v]\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 2; i < argc; ++i)
    {
        if (argv[i][0] != '-')
        { // If it's not a flag ignore
            continue;
        }
        switch (argv[i][1])
        {
        case 'p':
            if (isValidPort(argv[i + 1]))
            {
                strcpy(portGS, argv[i + 1]);
            }
            else
            {
                fprintf(stderr, "Invalid port number. Usage: ./GS word_file [-p GSport] [-v]\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'v':
            verbose = VERBOSE_ON;
            break;
        default:
            fprintf(stderr, "Invalid server program arguments. Usage: ./GS word_file [-p GSport] [-v]\n");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    parseArgs(argc, argv);
    createUDPTCPConnections();
    // Have 2 separate processes handling different operations
    pid_t pid = fork();
    if (pid == 0)
    { // Set child process to initiate UDP operations
        initiateServerTCP();
    }
    else if (pid > 0)
    { // Set parent process to initiate TCP operations
        initiateServerUDP();
    }
    else
    {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}