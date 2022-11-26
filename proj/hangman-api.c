#include "hangman-api.h"
#include "hangman-api-constants.h"

#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

int validRegex(char *buf, char *reg)
{
    int reti;
    regex_t regex;
    reti = regcomp(&regex, reg, REG_EXTENDED);
    if (reti)
    { // If the regex didn't compile
        fprintf(stderr, "[-] Internal error on parsing regex. Please try again later and/or contact the developers.\n");
        return 0;
    }
    if (regexec(&regex, buf, (size_t)0, NULL, 0))
    { // If the buffer doesn't match with the pattern
        regfree(&regex);
        return 0;
    }
    regfree(&regex); // Free allocated memory
    return 1;
}

int isValidAddress(char *address)
{ // First pattern is for hostname and second one is for IPv4 addresses/IP addresses
    return (validRegex(address, "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9-]*[a-zA-Z0-9]).)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9-]*[A-Za-z0-9])$") ||
            validRegex(address, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"));
}

int isValidPort(char *port)
{ // Ports range from 0 to 65535
    return validRegex(port, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
}

void closeUDPSocket(int fdUDP, struct addrinfo *resUDP)
{
    freeaddrinfo(resUDP);
    close(fdUDP);
}

void closeTCPSocket(int fdTCP, struct addrinfo *resTCP)
{
    freeaddrinfo(resTCP);
    close(fdTCP);
}

int isValidPLID(char *PLID)
{
    return validRegex(PLID, "^[0-9]{6}$");
}

int isValidPlay(char *play)
{
    return validRegex(play, "^[a-zA-Z]$");
}

int isValidGuess(char *guess)
{
    return validRegex(guess, "^[a-zA-Z]+$");
}

int timerOn(int fd)
{
    struct timeval timeout;
    memset((char *)&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 3;
    return (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(struct timeval)));
}

int timerOff(int fd)
{
    struct timeval timeout;
    memset((char *)&timeout, 0, sizeof(timeout));
    return (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(struct timeval)));
}

int sendTCPMessage(int fd, char *message)
{
    int bytesSent = 0;
    ssize_t nSent;
    size_t messageLen = strlen(message);
    while (bytesSent < messageLen)
    { // Send initial message
        nSent = write(fd, message + bytesSent, messageLen - bytesSent);
        if (nSent == -1)
        {
            perror("[-] Failed to write on TCP");
            return nSent;
        }
        bytesSent += nSent;
    }
    return bytesSent;
}

int readTCPMessage(int fd, char *message, int maxSize)
{
    int bytesRead = 0;
    ssize_t n;

    while (bytesRead < maxSize)
    {
        if (timerOn(fd) == -1)
        {
            perror("[-] Failed to start TCP timer");
            return -1;
        }
        n = read(fd, message + bytesRead, maxSize - bytesRead);

        // As soon as we read the first byte, turn off timer
        if (timerOff(fd) == -1)
        {
            perror("[-] Failed to turn off TCP timer");
            return -1;
        }

        if (n == 0)
        {
            break; // Peer has performed an orderly shutdown -> POSSIBLE message complete
        }
        if (n == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                perror("[-] TCP socket timed out while reading. Program will now exit.\n");
                return 0;
            }
            else
            {
                perror("[-] Failed to receive from server on TCP");
                return n;
            }
        }
        bytesRead += n;
    }
    return bytesRead;
}