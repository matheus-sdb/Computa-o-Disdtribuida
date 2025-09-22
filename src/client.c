#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/proto.h"

static int connect_tcp(const char *host, int port) {
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, portstr, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int sfd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sfd);
        sfd = -1;
    }
    freeaddrinfo(res);
    return sfd;
}

int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int port = DEFAULT_PORT;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "invalid port: %s\n", argv[2]);
            return 1;
        }
    }

    int sfd = connect_tcp(host, port);
    if (sfd < 0) {
        fprintf(stderr, "failed to connect to %s:%d\n", host, port);
        return 1;
    }

    fprintf(stderr, "[client] connected to %s:%d\n", host, port);

    FILE *sock = fdopen(sfd, "r+");
    if (!sock) {
        perror("fdopen");
        close(sfd);
        return 1;
    }
    setvbuf(sock, NULL, _IOLBF, 0);

    char line[MAX_LINE];
    while (1) {
        if (!fgets(line, sizeof(line), stdin)) break;
        // Ensure newline
        size_t n = strlen(line);
        if (n == 0 || line[n-1] != '\n') {
            if (n < sizeof(line)-1) line[n++] = '\n', line[n] = '\0';
        }
        fputs(line, sock);
        fflush(sock);

        // If user typed QUIT, we still try to read server's reply then exit
        if (strncasecmp(line, "QUIT", 4) == 0) {
            char resp[MAX_LINE];
            if (fgets(resp, sizeof(resp), sock)) {
                fputs(resp, stdout);
            }
            break;
        }

        char resp[MAX_LINE];
        if (!fgets(resp, sizeof(resp), sock)) {
            fprintf(stderr, "[client] server closed connection\n");
            break;
        }
        fputs(resp, stdout);
    }

    fclose(sock); // also closes sfd
    return 0;
}
