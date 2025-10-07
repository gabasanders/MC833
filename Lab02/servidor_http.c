#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define LISTENQ      10
#define MAXDATASIZE  256
#define MAXLINE      4096

// Wrappers
int Socket(int domain, int type, int protocol) {
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    return listenfd;
}
int Close(int fd) {
    return close(fd);
}
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (bind(sockfd, addr, addrlen) == -1) {
        perror("bind");
        Close(sockfd);
        exit(1);
    }
}
void Listen(int sockfd, int backlog) {
    if (listen(sockfd, LISTENQ) == -1) {
        perror("listen");
        Close(sockfd);
        exit(1);
    }
}
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return accept(sockfd, addr, addrlen);
}
int Getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return getsockname(sockfd, addr, addrlen);
}
int Getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return getpeername(sockfd, addr, addrlen);
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
    // Imprime mensagem recebida
    printf("[CLI MSG] %s", (char*)buf);
    return n;
}
ssize_t Write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

void doit(int connfd) {
    struct sockaddr_in remoteaddr;
    socklen_t len = sizeof(remoteaddr);
    if (Getpeername(connfd, (struct sockaddr *)&remoteaddr, &len) == 0){
        char remoteip[INET_ADDRSTRLEN];
        unsigned short remoteport;

        inet_ntop(AF_INET, &(remoteaddr.sin_addr), remoteip, sizeof(remoteip));
        remoteport = ntohs(remoteaddr.sin_port);

        printf("local: %s:%hu\n",remoteip, remoteport);
    }

    // lê o input vindo do cliente
    char client_input[MAXLINE + 1];
    Read(connfd, client_input, MAXLINE);
    

    const char* response;
    if (strncmp(client_input, "GET / HTTP/1.0", 14) == 0 ||
        strncmp(client_input, "GET / HTTP/1.1", 14) == 0) {

        // Resposta em caso positivo
        response =
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 91\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><head><title>MC833</title></head><body><h1>MC833 TCP Concorrente </h1></body></html>\r\n";
    } else {
        response = "400 Bad Request\r\n";
    }

    (void)Write(connfd, response, strlen(response));
    Close(connfd);
    exit(0);
}

int main(int argc, char *argv[]) {
    pid_t pid;
    int listenfd, connfd;
    struct sockaddr_in servaddr;

    // socket
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (argc > 1){
        int selected_port = atoi(argv[1]);
        servaddr.sin_port = htons(selected_port);
    }else{
        servaddr.sin_port = 0;              
    }

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    
    // Descobrir porta real e divulgar em arquivo server.info
    struct sockaddr_in bound; socklen_t blen = sizeof(bound);
    if (Getsockname(listenfd, (struct sockaddr*)&bound, &blen) == 0) {
        unsigned short p = ntohs(bound.sin_port);
        printf("[SERVIDOR] Escutando em 127.0.0.1:%u\n", p);
        FILE *f = fopen("server.info", "w");
        if (f) { fprintf(f, "IP=127.0.0.1\nPORT=%u\n", p); fclose(f); }
        fflush(stdout);
    }

    // listen
    Listen(listenfd, LISTENQ);

    // laço: aceita clientes, envia banner e fecha a conexão do cliente
    for (;;) {
        connfd = Accept(listenfd, NULL, NULL);
        if (connfd == -1) {
            perror("accept");
            continue; // segue escutando
        }

        //cria processo filho
        if ((pid = fork()) == 0) {
            Close(listenfd);
            //doit -> executa processamento
            doit(connfd);
        }

        Close(connfd); // fecha só a conexão aceita; servidor segue escutando
    }

    return 0;
}
