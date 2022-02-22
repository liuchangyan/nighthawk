
#include<event2/event-config.h>
 
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
 
#include<event.h>
#include<event2/util.h>
 
int tcp_connect_server(const char* server_ip, int port);
 
void cmd_msg_cb(int fd, short events, void* arg);
 
void socket_read_cb(int fd, short events, void* arg);
 
int main(int argc, char** argv) {
	if(argc < 3) {
		printf("please input 2 parameter \n");
		return -1;
	}
 
	//两个参数依次是服务器端的IP地址，端口号
	int sockfd = tcp_connect_server(argv[1],atoi(argv[2]));
	if( sockfd == -1) {
		perror("tcp_connect error ");
		return -1;
	}
 
	printf("connect to server successful \n");

    struct event_config *config = event_config_new();
    //显示支持的网络模式
    const char **methods = 	event_get_supported_methods();
    printf( "support methods\n " );
    for(int i = 0; methods[i] != NULL; i++)
    {
    	printf("%c\n", methods[i]);
    }
    event_config_avoid_method(config, "epoll");
    // event_config_avoid_method(config, "select");
    struct event_base *base =  event_base_new_with_config(config);
    //config一旦配置好就不需要在使用了
    //也就是所所有的配置信息需要在这之前进行销毁
    // event_config_free(config);

 
	// struct event_base* base = event_base_new();
 
	//监听读事件
	struct event* ev_sockfd = event_new(base,sockfd,EV_READ | EV_PERSIST, socket_read_cb, NULL);
	
	event_add(ev_sockfd, NULL);	//正式添加事件
 
	//监听终端输入事件
	struct event* ev_cmd = event_new(base,STDIN_FILENO,EV_READ | EV_PERSIST, cmd_msg_cb, (void*)&sockfd);
																			//回调函数：参数是：fd,event,arg
 
	event_add(ev_cmd,NULL);		//添加事件
 
	event_base_dispatch(base);	//进入循环，等待就绪事件并执行事件处理
	
	printf("finished\n");
	
	return 0;
}
 
 
void cmd_msg_cb(int fd, short events, void* arg) { // fd:STDIN_FILENO,event:EV_READ | EV_PERSIST,arg:sockfd;
											      //该回调函数作用是从标准输入读到msg,再把msg的数据发送给sockfd(服务器端)
	char msg[1024];
 
	int ret = read(fd,msg,sizeof(msg));			//ret是读取的长度
	if(ret <= 0) {
		perror("read fail");
		exit(1);
	}
 
	int sockfd = *((int*)arg);
 
	//把终端的消息发送给服务器端
	//为了简单起见，不考虑写一半数据的情况
	
	write(sockfd,msg,ret);
}
 
void socket_read_cb(int fd, short events, void* arg) { //fd:sockfd,event:EV_READ | EV_PERSIST,arg是空
	char msg[1024];
 
	//为了简单起见，不考虑写一半数据的情况 
	int len = read(fd,msg,sizeof(msg)-1);
	if(len <= 0) {
		perror("read fail");
		exit(1);
	}
 
	msg[len] = '\0';
	
	printf("recv %s fron server\n",msg);
}
 
typedef struct sockaddr SA;
int tcp_connect_server(const char* server_ip, int port) {
	int sockfd, status,save_errno;
	struct sockaddr_in server_addr;
 
	memset(&server_addr, 0, sizeof(server_addr) );
 
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	status = inet_aton(server_ip,&server_addr.sin_addr);	//将IP地址转换为网络字节序整数
 
 
	if(status == 0)	//the server_ip is not valid value
	{
		errno = EINVAL;
		return -1;
	}
 
	sockfd = socket(PF_INET,SOCK_STREAM,0);
	if (sockfd == -1)
		return sockfd;
 
	status = connect(sockfd,(SA*)&server_addr, sizeof(server_addr) );
 
	if(status == -1) {
		save_errno = errno;
		close(sockfd);
		errno = save_errno;	//the close may be error 
		return -1;
	}
 
	evutil_make_socket_nonblocking(sockfd);	//设置成不阻塞
 
	return sockfd;
}




