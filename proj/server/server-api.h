#ifndef SERVER_API_H
#define SERVER_API_H

#include "server-operations.h"
#include "../constants.h"
#include "../common.h"

char *processServerUDP(char *message);

void processServerTCP(int fd, char *command);

#endif