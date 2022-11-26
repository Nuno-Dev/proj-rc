#ifndef HANGMAN_API_H
#define HANGMAN_API_H

#include "hangman-api-constants.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int isValidAddress(char *address);

int isValidPort(char *port);

/* Parse client commands */
int parseClientCommand(char *command);

int isValidPLID(char *PLID);

int isValidPlay(char *play);

int isValidGuess(char *guess);

int sendTCPMessage(int fd, char *message);

int receiveTCPMessage(int fd, char *message);

#endif