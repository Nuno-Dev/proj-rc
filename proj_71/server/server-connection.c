#include "server-connection.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

/*Server information variables */
char portGS[GS_PORT_SIZE] = GS_DEFAULT_PORT;
int verbose = VERBOSE_OFF;

/* UDP Socket related variables */
int fdUDP;
struct addrinfo hintsUDP, *resUDP;

/* TCP Socket related variables */
int fdTCP;
struct addrinfo hintsTCP, *resTCP;

void createUDPTCPConnections()
{
    // UDPs
    fdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (fdUDP == -1)
    {
        perror("Server UDP socket failed to create");
        exit(EXIT_FAILURE);
    }
    memset(&hintsUDP, 0, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_INET;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    hintsUDP.ai_flags = AI_PASSIVE;
    int errcode = getaddrinfo(NULL, portGS, &hintsUDP, &resUDP);
    if (errcode != 0)
    {
        perror("Failed on UDP address translation");
        close(fdUDP);
        exit(EXIT_FAILURE);
    }
    int n = bind(fdUDP, resUDP->ai_addr, resUDP->ai_addrlen);
    if (n == -1)
    {
        perror("Failed to bind UDP server");
        closeUDPSocket(fdUDP, resUDP);
        exit(EXIT_FAILURE);
    }

    // TCP
    fdTCP = socket(AF_INET, SOCK_STREAM, 0);
    if (fdTCP == -1)
    {
        perror("Server TCP socket failed to create");
        closeUDPSocket(fdUDP, resUDP);
        exit(EXIT_FAILURE);
    }
    memset(&hintsTCP, 0, sizeof(hintsTCP));
    hintsTCP.ai_family = AF_INET;
    hintsTCP.ai_socktype = SOCK_STREAM;
    hintsTCP.ai_flags = AI_PASSIVE;
    int err;
    err = getaddrinfo(NULL, portGS, &hintsTCP, &resTCP);
    if (err != 0)
    {
        perror("Failed on TCP address translation");
        closeUDPSocket(fdUDP, resUDP);
        close(fdTCP);
        exit(EXIT_FAILURE);
    }
    err = bind(fdTCP, resTCP->ai_addr, resTCP->ai_addrlen);
    if (err == 1)
    {
        perror("Server TCP socket failed to bind");
        closeUDPSocket(fdUDP, resUDP);
        closeTCPSocket(fdTCP, resTCP);
        exit(EXIT_FAILURE);
    }
    err = listen(fdTCP, GS_LISTENQUEUE_SIZE);
    if (err == -1)
    {
        perror("Failed to prepare TCP socket to accept connections");
        closeUDPSocket(fdUDP, resUDP);
        closeTCPSocket(fdTCP, resTCP);
        exit(EXIT_FAILURE);
    }

    if (verbose == VERBOSE_OFF)
    { // Close stderr and stdout if verbose is off
        if (fclose(stdout) == -1)
        {
            perror("Failed to prepare TCP socket to accept connections");
            closeUDPSocket(fdUDP, resUDP);
            closeTCPSocket(fdTCP, resTCP);
            exit(EXIT_FAILURE);
        }
        if (fclose(stderr) == -1)
        {
            perror("Failed to prepare TCP socket to accept connections");
            closeUDPSocket(fdUDP, resUDP);
            closeTCPSocket(fdTCP, resTCP);
            exit(EXIT_FAILURE);
        }
    }

    // If verbose is on print out where the server is running and at which port it's listening to
    char hostname[GS_IP_SIZE + 1];
    if (gethostname(hostname, GS_IP_SIZE) == -1)
    {
        fprintf(stderr, "Failed to get Server hostname.\n");
        closeUDPSocket(fdUDP, resUDP);
        closeTCPSocket(fdTCP, resTCP);
        exit(EXIT_FAILURE);
    }
    printf("Server <%s> started.\nCurrently listening in port %s for UDP and TCP connections...\n\n", hostname, portGS);
}

void logVerbose(char *clientBuf, struct sockaddr_in s)
{
    printf("Client <%s> in port %d sent: %s\n", inet_ntoa(s.sin_addr), ntohs(s.sin_port), clientBuf);
}

void initiateServerUDP()
{
    char clientBuf[CLIENT_MESSAGE_UDP_SIZE];
    char *serverBuf;
    struct sockaddr_in cliaddr;
    socklen_t addrlen;
    ssize_t n;
    while (1)
    {
        addrlen = sizeof(cliaddr);
        n = recvfrom(fdUDP, clientBuf, sizeof(clientBuf), 0, (struct sockaddr *)&cliaddr, &addrlen);
        if (n == -1)
        {
            perror("(UDP) Server failed on recvfrom");
            closeUDPSocket(fdUDP, resUDP);
            exit(EXIT_FAILURE);
        }
        if (clientBuf[n - 1] != '\n')
        { // Every request/reply must end with a newline \n
            n = sendto(fdUDP, ERROR_MSG, ERROR_MSG_SIZE, 0, (struct sockaddr *)&cliaddr, addrlen);
            if (n == -1)
            {
                perror("(UDP) Server failed on sendto");
                closeUDPSocket(fdUDP, resUDP);
                exit(EXIT_FAILURE);
            }
        }
        clientBuf[n - 1] = '\0';
        char commandCode[SERVER_COMMAND_SIZE];
        strncpy(commandCode, clientBuf, SERVER_COMMAND_SIZE - 1);
        commandCode[SERVER_COMMAND_SIZE - 1] = '\0';
        printf("Client @ %s:%d sent %s command.\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), commandCode);
        serverBuf = processServerUDP(clientBuf);
        // print server response
        printf("Server response: %s", serverBuf);
        n = sendto(fdUDP, serverBuf, strlen(serverBuf), 0, (struct sockaddr *)&cliaddr, addrlen);
        if (n == -1)
        {
            perror("(UDP) Server failed on sendto");
            free(serverBuf);
            closeUDPSocket(fdUDP, resUDP);
            exit(EXIT_FAILURE);
        }
        free(serverBuf);
    }
}

void initiateServerTCP()
{
    struct sockaddr_in cliaddr;
    socklen_t addrlen;
    int newFdTCP;
    pid_t pid;
    int ret;
    while (1)
    {
        addrlen = sizeof(cliaddr);
        if ((newFdTCP = accept(fdTCP, (struct sockaddr *)&cliaddr, &addrlen)) == -1)
        { // If this connect failed to accept let's continue to try to look for new ones
            continue;
        }
        if ((pid = fork()) == 0)
        {
            close(fdTCP);
            char commandCode[SERVER_COMMAND_SIZE];
            int n = readTCPMessage(newFdTCP, commandCode, SERVER_COMMAND_SIZE);
            if (n == -1)
            {
                close(newFdTCP);
                exit(EXIT_FAILURE);
            }
            if (commandCode[SERVER_COMMAND_SIZE - 1] != ' ')
            {
                sendTCPMessage(newFdTCP, ERROR_MSG);
                close(newFdTCP);
                exit(EXIT_FAILURE);
            }
            commandCode[SERVER_COMMAND_SIZE - 1] = '\0';
            printf("Client <%s> :%d sent %s command.\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), commandCode);
            processServerTCP(newFdTCP, commandCode);
            close(newFdTCP);
            exit(EXIT_SUCCESS);
        }
        do
        {
            ret = close(newFdTCP);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1)
        {
            fprintf(stderr, "Failed to properly close new descriptor.\n");
            closeTCPSocket(fdTCP, resTCP);
            exit(EXIT_FAILURE);
        }
    }
}