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
    std::cout << "This is client" << std::endl;
    // socket
    int client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == -1) {
        std::cout << "Error: socket" << std::endl;
        return 0;
    }
    // connect
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(10002);
    serverAddr.sin_addr.s_addr = inet_addr("172.41.1.11");
    if (connect(client, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cout << "Error: connect" << std::endl;
        return 0;
    }
    std::cout << "...connect" << std::endl;
    char data[255];
    char buf[255];
    while (true) {
        std::cin >> data;
        send(client, data, strlen(data), 0);
        if (strcmp(data, "exit") == 0) {
            std::cout << "...disconnect" << std::endl;
            break;
        }
        memset(buf, 0, sizeof(buf));
        int len = recv(client, buf, sizeof(buf), 0);
        buf[len] = '\0';
        std::cout << buf << std::endl;
    }
    close(client);
    return 0;
}
