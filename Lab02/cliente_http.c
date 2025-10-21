// cliente_base.c — conecta, lê banner do servidor e fecha
// Compilação: gcc -Wall cliente_base.c -o cliente
// Uso: ./cliente [IP] [PORT]

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXLINE 4096

int Close(int fd) {
    return close(fd);
}
int Socket(int domain, int type, int protocol) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    return sockfd;
}
void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (connect(sockfd, addr, addrlen) < 0) {
        perror("connect error");
        Close(sockfd);
        exit(1);
    }
}
int Getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return getsockname(sockfd, addr, addrlen);
}
ssize_t Read(int fd, void *buf, size_t count) {
    ssize_t n = read(fd, buf, count);
    if (buf && count > 0) {
        if (n >= 0 && (size_t)n < count) {
            ((char*)buf)[n] = '\0';
        } else {
            ((char*)buf)[count - 1] = '\0';
        }
    }
    if (n > 0) {
        fputs((char*)buf, stdout);
        fflush(stdout);
    }
    return n;
}
ssize_t Write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}
static inline void addr_to_ip_port(const struct sockaddr_in *sa, char *ip, size_t iplen, unsigned short *port) {
    inet_ntop(AF_INET, &sa->sin_addr, ip, iplen);
    *port = ntohs(sa->sin_port);
}

void read_input(char *buf) {
    printf("Lendo input do usuario...\n");
    if (fgets(buf, MAXLINE + 1, stdin) != NULL) {
        // ok
    } else {
        printf("Erro ao ler input");
    }

}

int main(int argc, char **argv) {
    int    sockfd;
    struct sockaddr_in servaddr;

    // IP/PORT (argumentos ou server.info)
    char ip[INET_ADDRSTRLEN];
    unsigned short port = 0;

    if (argc >= 2) strncpy(ip, argv[1], sizeof(ip)-1);
    if (argc >= 3) port = (unsigned short)atoi(argv[2]);

    // socket
    sockfd = Socket(AF_INET, SOCK_STREAM, 0);

    // connect
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton error");
        Close(sockfd);
        return 1;
    }
    Connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    struct sockaddr_in localaddr;
    socklen_t len = sizeof(localaddr);
    if (Getsockname(sockfd, (struct sockaddr*)&localaddr, &len) == 0){
        char localip[INET_ADDRSTRLEN];
        unsigned short localport;

        //addr_to_ip_port(&localaddr, lip, sizeof(lip), &lport);
        inet_ntop(AF_INET, &(localaddr.sin_addr), localip, sizeof(localip));
        localport = ntohs(localaddr.sin_port);

        printf("local IP/port: %s:%hu\n", localip, localport);
        printf("remote IP/port: %s:%hu\n", ip, port);
    }
    
    char server_response[MAXLINE + 1];

    // lê o input do usuário
    char *http_request = "GET / HTTP/1.0\r\n"
                            "Host: teste\r\n"
                            "\r\n";

    (void)Write(sockfd, http_request, MAXLINE + 1);

    Read(sockfd, server_response, MAXLINE);

    sleep(5);

    Close(sockfd);
    return 0;
}
