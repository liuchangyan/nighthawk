#include <stdio.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#define PORT 9999
#define IP "172.41.1.11"
void read_cb(struct bufferevent *bev, void *arg)
{
	char buf[1024] = {0};
	//memset(buf, '\0', sizeof(buf));
	bufferevent_read(bev, buf, sizeof(buf));
	printf("server send data: %s\n", buf);	
}
void write_cb(struct bufferevent *bev, void *arg)
{
	printf("i am client,write data.... done!\n");
}
void event_cb(struct bufferevent *bev, short events, void *arg)
{
	if(events & BEV_EVENT_EOF)
	{
		printf("connection closed\n");
	}
	else if(events & BEV_EVENT_ERROR)
	{
		printf("error\n");
	}
	else if(events & BEV_EVENT_CONNECTED)
	{
		printf("已经连接服务器\n");
		return;
	}
	printf("free bufferevent...\n");
	bufferevent_free(bev);
}


void read_terminal_cb(evutil_socket_t fd, short what, void *arg)
{
	char buf[BUFSIZ] = {0};
	int len = read(fd, buf, sizeof(buf));
	struct bufferevent *bev = (struct bufferevent*) arg;
	bufferevent_write(bev, buf, len);
}
int main()
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct event_base *base = event_base_new();
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	inet_pton(AF_INET, IP, &serv_addr.sin_addr.s_addr);
	
	int ret = bufferevent_socket_connect(bev, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	
	bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);
//	bufferevent_setcb(bev, read_cb, write_cb, NULL, NULL);
	//启用bufferevent读缓冲，（默认创建时，读缓冲处于关闭状态，写缓冲处于开启状态）
	bufferevent_enable(bev, EV_READ);

	if(ret < 0)
	{
		perror("bufferevent_socket_connect error");
		return -1;
	}
	//设置并监听终端输入事件
	struct event *ev = event_new(base,
			STDIN_FILENO,
			EV_READ | EV_PERSIST,
			read_terminal_cb, bev);
	//添加事件
	event_add(ev, NULL);

	event_base_dispatch(base);
	//释放资源
	event_free(ev);
	bufferevent_free(bev);
	event_base_free(base);
	return 0;
}


