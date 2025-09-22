#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h>

#include "../include/proto.h"

static volatile sig_atomic_t keep_running = 1;

static void on_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

static void on_sigchld(int sig) {
    (void)sig;
    // Reap all finished children without blocking
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static void trim_nl(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[--n] = '\0';
    }
}

// Convert string to upper in-place
static void str_toupper(char *s) {
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

static int is_op_keyword(const char *s) {
    return strcmp(s, "ADD") == 0 || strcmp(s, "SUB") == 0 ||
           strcmp(s, "MUL") == 0 || strcmp(s, "DIV") == 0;
}

static int parse_double(const char *s, double *out) {
    if (!s || !out) return 0;
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s) return 0;
    // Skip trailing spaces
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

// Format double with up to 6 decimal places, trimming trailing zeros
static void format_double(double v, char *buf, size_t bufsz) {
    // fixed 6 decimals, then trim
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%.6f", v);
    // trim zeros
    char *p = tmp + strlen(tmp) - 1;
    while (p > tmp && *p == '0') { *p-- = '\0'; }
    if (p > tmp && *p == '.') { *p = '\0'; }
    snprintf(buf, bufsz, "%s", tmp);
}

static void handle_client(int cfd, struct sockaddr_in *peer) {
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer->sin_addr, addr, sizeof(addr));
    int port = ntohs(peer->sin_port);
    fprintf(stderr, "[server] connection from %s:%d\n", addr, port);

    FILE *in = fdopen(cfd, "r+");
    if (!in) {
        dprintf(cfd, RESP_ERR " " ERR_SRV " fdopen_failed\n");
        close(cfd);
        return;
    }
    setvbuf(in, NULL, _IOLBF, 0); // line buffered

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), in) != NULL) {
        trim_nl(line);
        if (line[0] == '\0') continue;

        // QUIT closes connection
        if (strcasecmp(line, "QUIT") == 0) {
            fprintf(stderr, "[server] %s:%d sent QUIT\n", addr, port);
            dprintf(cfd, RESP_OK " bye\n");
            break;
        }

        // Tokenize (up to 3 tokens enough for both syntaxes)
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", line);

        char *t1 = strtok(tmp, " \t");
        char *t2 = strtok(NULL, " \t");
        char *t3 = strtok(NULL, " \t");
        char *extra = strtok(NULL, " \t");

        if (!t1 || !t2 || !t3 || extra) {
            dprintf(cfd, RESP_ERR " " ERR_INV " formato_invalido\n");
            continue;
        }

        // Two possible syntaxes:
        // 1) Prefix: OP A B
        // 2) Infix:  A <op> B  where <op> in {+, -, *, /}
        double a = 0.0, b = 0.0;
        char opkw[8] = {0};

        if (is_op_keyword((char[]){(char)toupper((unsigned char)t1[0]),0})) {
            // quick check fell short; do full uppercase compare
        }

        // Try prefix first
        char opbuf[8];
        snprintf(opbuf, sizeof(opbuf), "%s", t1);
        str_toupper(opbuf);
        int prefix = is_op_keyword(opbuf);

        if (prefix) {
            if (!parse_double(t2, &a) || !parse_double(t3, &b)) {
                dprintf(cfd, RESP_ERR " " ERR_INV " numero_invalido\n");
                continue;
            }
            snprintf(opkw, sizeof(opkw), "%s", opbuf);
        } else {
            // Try infix
            if (!parse_double(t1, &a) || !parse_double(t3, &b)) {
                dprintf(cfd, RESP_ERR " " ERR_INV " numero_invalido\n");
                continue;
            }
            if (strcmp(t2, "+") == 0) snprintf(opkw, sizeof(opkw), "ADD");
            else if (strcmp(t2, "-") == 0) snprintf(opkw, sizeof(opkw), "SUB");
            else if (strcmp(t2, "*") == 0) snprintf(opkw, sizeof(opkw), "MUL");
            else if (strcmp(t2, "/") == 0) snprintf(opkw, sizeof(opkw), "DIV");
            else {
                dprintf(cfd, RESP_ERR " " ERR_INV " operador_invalido\n");
                continue;
            }
        }

        double result = 0.0;
        if (strcmp(opkw, "ADD") == 0) {
            result = a + b;
        } else if (strcmp(opkw, "SUB") == 0) {
            result = a - b;
        } else if (strcmp(opkw, "MUL") == 0) {
            result = a * b;
        } else if (strcmp(opkw, "DIV") == 0) {
            if (b == 0.0) {
                dprintf(cfd, RESP_ERR " " ERR_ZDV " divisao_por_zero\n");
                continue;
            }
            result = a / b;
        } else {
            dprintf(cfd, RESP_ERR " " ERR_INV " operador_invalido\n");
            continue;
        }

        char rbuf[128];
        format_double(result, rbuf, sizeof(rbuf));
        dprintf(cfd, RESP_OK " %s\n", rbuf);
    }

    fclose(in);
    fprintf(stderr, "[server] connection %s:%d closed\n", addr, port);
}

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C"); // enforce '.' decimal

    int port = DEFAULT_PORT;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    struct sigaction sa_int = {0};
    sa_int.sa_handler = on_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_chld = {0};
    sa_chld.sa_handler = on_sigchld;
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sfd);
        return 1;
    }

    if (listen(sfd, BACKLOG) < 0) {
        perror("listen");
        close(sfd);
        return 1;
    }

    fprintf(stderr, "[server] listening on 0.0.0.0:%d\n", port);

    while (keep_running) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int cfd = accept(sfd, (struct sockaddr*)&peer, &plen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue;
        } else if (pid == 0) {
            // child
            close(sfd);
            handle_client(cfd, &peer);
            close(cfd);
            _exit(0);
        } else {
            // parent
            close(cfd);
        }
    }

    close(sfd);
    fprintf(stderr, "[server] shutting down\n");
    return 0;
}
