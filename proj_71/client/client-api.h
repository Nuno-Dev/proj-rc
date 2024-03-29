#ifndef CLIENT_API_H
#define CLIENT_API_H

#include "../common.h"
#include "../constants.h"

/* GS Server information variables */
extern char GSIP[GS_IP_SIZE];
extern char GSport[GS_PORT_SIZE];

void createUDPSocket();

void sendUDPMessage(char *message, int command);

void processUDPReply(char *message);

void clientStart(char **tokenList, int numTokens);
void clientGuess(char **tokenList, int numTokens);
void clientPlay(char **tokenList, int numTokens);
void clientScoreboard(int numTokens);
void clientHint(int numTokens);
void clientState(int numTokens);
void clientQuit(int numTokens);
void clientExit(int numTokens);
void clientKillGame(char **tokenList, int numTokens);
void clientKillDirectory(char **tokenList, int numTokens);

#endif