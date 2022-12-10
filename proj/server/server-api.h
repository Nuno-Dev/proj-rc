#ifndef SERVER_API_H
#define SERVER_API_H

#include "server-utils/server-operations.h"
#include "../constants.h"
#include "../common.h"

char *processClientUDP(char *message);

void processClientTCP(int fd, char *command);

#endif