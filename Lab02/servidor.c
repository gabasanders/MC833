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

/* Wrappers (sem inline). 
   Mantêm assinaturas equivalentes e comportamento.
   Pequena conveniência: Read() sempre deixa o buffer terminado em '\0'. */
int Socket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}
int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}
int Listen(int sockfd, int backlog) {
    return listen(sockfd, backlog);
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
    /* Conveniência: garanta terminação para uso com "%s" */
    if (buf && count > 0) {
        if (n >= 0 && (size_t)n < count) {
            ((char*)buf)[n] = '\0';
        } else {
            ((char*)buf)[count - 1] = '\0';
        }
    }
    return n;
}
ssize_t Write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}
int Close(int fd) {
    return close(fd);
}

void doit(int connfd) {
    struct sockaddr_in remoteaddr;
    socklen_t len = sizeof(remoteaddr);
    if (Getpeername(connfd, (struct sockaddr*)&remoteaddr, &len) == 0){
        char remoteip[INET_ADDRSTRLEN];
        unsigned short remoteport;

        inet_ntop(AF_INET, &(remoteaddr.sin_addr), remoteip, sizeof(remoteip));
        remoteport = ntohs(remoteaddr.sin_port);

        printf("local: %s:%hu\n",remoteip, remoteport);
    }

    // envia banner "Hello + Time"
    char buf[MAXDATASIZE];
    time_t ticks = time(NULL); // ctime() já inclui '\n'
    snprintf(buf, sizeof(buf), "Hello from server!\nTime: %.24s\r\n", ctime(&ticks));
    (void)Write(connfd, buf, strlen(buf));

    // lê o input vindo do cliente
    char client_input[MAXLINE + 1];
    Read(connfd, client_input, MAXLINE);
    
    printf("[CLI MSG] %s", client_input);

    const char* response;
    if (strncmp(client_input, "GET / HTTP/1.0", 14) == 0 ||
        strncmp(client_input, "GET / HTTP/1.1", 14) == 0) {

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
    if ((listenfd = Socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (argc > 1){
        int selected_port = atoi(argv[1]);
        servaddr.sin_port = htons(selected_port);
    }else{
        servaddr.sin_port = 0;              
    }

    if (Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind");
        Close(listenfd);
        return 1;
    }
    
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
    if (Listen(listenfd, LISTENQ) == -1) {
        perror("listen");
        Close(listenfd);
        return 1;
    }

    // laço: aceita clientes, envia banner e fecha a conexão do cliente
    for (;;) {
        connfd = Accept(listenfd, NULL, NULL);
        if (connfd == -1) {
            perror("accept");
            continue; // segue escutando
        }

        if ((pid = fork()) == 0) {
            Close(listenfd);
            //doit
            doit(connfd);
        }

        Close(connfd); // fecha só a conexão aceita; servidor segue escutando
    }

    return 0;
}
