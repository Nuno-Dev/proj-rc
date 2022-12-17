#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include "../constants.h"
#include "../common.h"
#include "server-api.h"

extern char portGS[GS_PORT_SIZE];
extern int verbose;

void createUDPTCPConnections();

void logVerbose(char *clientBuf, struct sockaddr_in s);

void initiateServerUDP();

void initiateServerTCP();

#endif