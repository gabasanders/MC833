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

static inline void addr_to_ip_port(const struct sockaddr_in *sa, char *ip, size_t iplen, unsigned short *port) {
    inet_ntop(AF_INET, &sa->sin_addr, ip, iplen);
    *port = ntohs(sa->sin_port);
}

int main(int argc, char **argv) {
    int    sockfd;
    struct sockaddr_in servaddr;

    // IP/PORT (argumentos ou server.info)
    char ip[INET_ADDRSTRLEN] = "127.0.0.1";
    unsigned short port = 0;

    if (argc >= 2) strncpy(ip, argv[1], sizeof(ip)-1);
    if (argc >= 3) port = (unsigned short)atoi(argv[2]);

    if (port == 0) {
        FILE *f = fopen("server.info", "r");
        if (f) {
            char line[128]; int got_p = 0;
            while (fgets(line, sizeof(line), f)) {
                (void)sscanf(line, "IP=%127s", ip);        // lê IP se houver, sem flag
                if (sscanf(line, "PORT=%hu", &port) == 1) got_p = 1;
            }
            fclose(f);
            if (!got_p) port = 0;
        }
        if (port == 0) {
            fprintf(stderr, "Uso: %s <IP> [PORT] (ou forneça server.info)\n", argv[0]);
            return 1;
        }

    }

    // socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        return 1;
    }

    // connect
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton error");
        close(sockfd);
        return 1;
    }
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        close(sockfd);
        return 1;
    }

    struct sockaddr_in localaddr;
    socklen_t len = sizeof(localaddr);
    if(getsockname(sockfd, (struct sockaddr*)&localaddr, &len) == 0){
        char localip[INET_ADDRSTRLEN];
        unsigned short localport;

        //addr_to_ip_port(&localaddr, lip, sizeof(lip), &lport);
        inet_ntop(AF_INET, &(localaddr.sin_addr), localip, sizeof(localip));
        localport = ntohs(localaddr.sin_port);

        printf("remote: %s:%hu\n",localip, localport);
    }


    // lê e imprime o banner (uma leitura basta neste cenário)
    char banner[MAXLINE + 1];
    ssize_t n = read(sockfd, banner, MAXLINE);
    if (n > 0) {
        banner[n] = 0;
        fputs(banner, stdout);
        fflush(stdout);
    }


    // le o input do usuario
    char user_input[MAXLINE + 1];
    printf("Lendo input do usuario...\n");
    if (fgets(user_input, MAXLINE + 1, stdin) != NULL) {

    }
    else {
        printf("Erro ao ler input");
    }

    (void)write(sockfd, user_input, MAXLINE+1);

    close(sockfd);
    return 0;
}