#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    std::cout << "This is server" << std::endl;
    // socket
    std::cout << "==========now server will call socket========="<<std::endl;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        std::cout << "Error: socket" << std::endl;
        return 0;
    }
    // bind
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(10000);
    addr.sin_addr.s_addr = inet_addr("172.41.1.18");
    std::cout << "==========now server will call bind========="<<std::endl;
    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cout << "Error: bind" << std::endl;
        return 0;
    }
    std::cout << "==========now server will call listen========="<<std::endl;
    // listen
    if(listen(listenfd, 5) == -1) {
        std::cout << "Error: listen" << std::endl;
        return 0;
    }
    // accept
    std::cout << "==========now server will call accept========="<<std::endl;
    int conn;
    char clientIP[INET_ADDRSTRLEN] = "172.41.1.7";
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    while (true) {
        std::cout << "...listening" << std::endl;
        conn = accept(listenfd, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (conn < 0) {
            std::cout << "Error: accept" << std::endl;
            continue;
        }
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "...connect " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

        char buf[255];
        while (true) {
            memset(buf, 0, sizeof(buf));
            std::cout << "==========now server will call recv========="<<std::endl;
            int len = recv(conn, buf, sizeof(buf), 0);
            buf[len] = '\0';
            if (strcmp(buf, "exit") == 0) {
                std::cout << "...disconnect " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;
                break;
            }
            std::cout << "==========now server will call send========="<<std::endl;
            std::cout << buf << std::endl;
            send(conn, buf, len, 0);
        }

        close(conn);
    }
    close(listenfd);
    return 0;
}
