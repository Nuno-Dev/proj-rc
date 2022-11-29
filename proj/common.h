#ifndef COMMON_H
#define COMMON_H

#include "constants.h"
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

int timerOn(int fd);

int timerOff(int fd);

void closeUDPSocket(int fdUDP, struct addrinfo *resUDP);

void closeTCPSocket(int fdTCP, struct addrinfo *resTCP);

#endif