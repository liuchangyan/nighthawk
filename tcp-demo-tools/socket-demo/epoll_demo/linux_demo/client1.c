#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

int main()
{
    struct sockaddr_in address;
    char ch = 'A';
    int cli_fd;
    int len;
    int ret;

    cli_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("172.41.1.11");
    address.sin_port = htons(10000);

    len = sizeof(address);

    ret = connect(cli_fd, (struct sockaddr *)&address, len);
    if(ret == -1)
    {
        perror("oops: client 2");
        exit(-1);
    }

    
    write(cli_fd, &ch, 1);

    ch = 0x00;

    read(cli_fd, &ch, 1);

    printf("the first time: char from server = %c\n", ch);

    sleep(5);

    
    ch = 'B';

    write(cli_fd, &ch, 1);

    ch = 0x00;

    read(cli_fd, &ch, 1);

    printf("the second time: char from server = %c\n", ch);

    close(cli_fd);

    return 0;
}

