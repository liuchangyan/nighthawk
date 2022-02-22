#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<errno.h>
#include<fcntl.h>
#define _BACKLOG_ 5
#define _BUF_SIZE_ 10240
#define _MAX_ 64
  
  typedef struct _data_buf
  {
      int fd;
      char buf[_BUF_SIZE_];
  }data_buf_t,*data_buf_p;
   static void usage(const char* proc)
  {
      printf("usage:%s[ip][port]\n",proc);
  }
  
  static int set_no_block(int fd) //用来设置非阻塞
  {
      int old_fl=fcntl(fd,F_GETFL);
      if(old_fl<0)
      {
          perror("perror");
          return -1;
      }
      if(fcntl(fd,F_SETFL,old_fl|O_NONBLOCK))
      {
          perror("fcntl");
          return -1;
      }
  
      return 0;
  }
     
  int read_data(int fd,char* buf,int size)//ET模式下读取数据，必须一次读完
  {
      assert(buf);                                                                                                                                                    
      int index=0;
      ssize_t _s=-1;
      while((_s=read(fd,buf+index,size-index))<size)
      {
          if(errno==EAGAIN)
          {
              break;
          }
         index += _s;
      }
      return index;
  }
  
  static int start(int port,char *ip)
  {
      assert(ip);
      int sock=socket(AF_INET,SOCK_STREAM,0);
      if(sock<0)
      {
          perror("socket");
          exit(1);
      }
 
      struct sockaddr_in local;
      local.sin_port=htons(port);
      local.sin_family=AF_INET;
      local.sin_addr.s_addr=inet_addr(ip);
  
      int opt=1;  //设置为接口复用
      setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
  
      if(bind(sock,(struct sockaddr*)&local,sizeof(local))<0)
      {
          perror("bind");
          exit(2);
      }
  
      if(listen(sock,_BACKLOG_)<0)
      {
          perror("listen");
          exit(3);
      }
      return sock;
  }
 
  static int epoll_server(int listen_sock)
  {
      int epoll_fd=epoll_create(256);//生成一个专用的epoll文件描述符
      if(epoll_fd<0)
      {
          perror("epoll_create");
          exit(1);
      }
     set_no_block(listen_sock); //设置监听套接字为非阻塞
     struct epoll_event ev;//用于注册事件
     struct epoll_event ret_ev[_MAX_];//数组用于回传要处理的事件
     int ret_num=_MAX_;
     int read_num=-1;
     ev.events=EPOLLIN|EPOLLET;
     ev.data.fd=listen_sock;
     if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_sock,&ev)<0)//用于控制某个文件描述符上的事件（注册，修改，删除）
     {
         perror("epoll_ctl");
         return -2;
     }
 
     int done=0;
     int i=0;
     int timeout=5000;
     struct sockaddr_in client;
     socklen_t len=sizeof(client);
     while(!done)
     {
         switch(read_num=epoll_wait(epoll_fd,ret_ev,ret_num,timeout))//用于轮寻I/O事件的发生
         {
             case  0:                                                                         
                 printf("time out\n");
                 break;
             case -1:
                 perror("epoll");
                 exit(2);
             default:
                 {
                     for(i=0;i<read_num;++i)
                     {
                         if(ret_ev[i].data.fd==listen_sock&&(ret_ev[i].events&EPOLLIN))
                         {
 
                             int fd=ret_ev[i].data.fd;
                             int new_sock=accept(fd,(struct sockaddr*)&client,&len);
                             if(new_sock<0)
                             {
                                 perror("accept");
                                 continue;
                             }
                             set_no_block(new_sock); //设置套接字为非阻塞                            
                             ev.events=EPOLLIN|EPOLLET;
                             ev.data.fd=new_sock;
                             epoll_ctl(epoll_fd,EPOLL_CTL_ADD,new_sock,&ev);
                             printf("get a new client...\n");
                         }
                         else  //normal sock
                         {
                             if(ret_ev[i].events&EPOLLIN)
                             {
                                 int fd=ret_ev[i].data.fd;
                                 data_buf_p mem=(data_buf_p)malloc(sizeof(data_buf_t));
                                 if(!mem)
                                 {  
                                     perror("malloc");                                                                      
                                     continue;
                                 }
                                 mem->fd=fd;
                                 memset(mem->buf,'\0',sizeof(mem->buf));
                                 ssize_t _s=read_data(mem->fd,mem->buf,sizeof(mem -> buf)-1);//一次读完
                                 if(_s>0)
                                 {
                                     mem->buf[_s-1]='\0';
                                     printf("client: %s\n",mem->buf);
                                     ev.events=EPOLLOUT|EPOLLET;
                                     ev.data.ptr=mem;
                                     epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev);
                                 }
                                 else if(_s==0)
                                 {
                                     printf("client close...\n");
                                     epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
                                     close(fd);
                                     free(mem);
                                 }
                                 else
                                 {
                                     continue;
                                 }
                             }
                             else if(ret_ev[i].events&EPOLLOUT)  //写事件准备就绪
                             {
                                     data_buf_p mem=(data_buf_p)ret_ev[i].data.ptr;
                                     char* msg="http/1.0 200 ok\r\n\r\nhello bit\r\n";
                                      int fd=mem->fd;
                                   
                                     write(fd,msg,strlen(msg));
                                     close(fd);
                                     epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,&ev);  //写完服务端直接退出
                                      free(mem);
                             }
                             else{
                                 //....
                             }
                         }
                     }
                 }
                 break;
         }
     }
 
 }
 
 int main(int argc,char* argv[])
 {
     if(argc!=3)
     {
         usage(argv[0]);
         return 1;
     }
 
     int port=atoi(argv[2]);
     char *ip=argv[1];
 
     int listen_sock=start(port,ip);
     epoll_server(listen_sock);
     close(listen_sock);
     return 0;
 }
