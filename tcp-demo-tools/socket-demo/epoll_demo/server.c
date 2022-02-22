#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

int epollfd;
#define SERVERPORT 8080
#define MAXCONN    5
#define MAXEVENTS  100
#define MAXLEN     1400
#define SOCK_PATH  "echo_socket"

typedef struct event_t {
    int      fd;
    uint32_t event;
    char     data[MAXLEN];
    int      length;
    int      offset;
} event_t;

static void event_set(int epollfd, int op, int fd, uint32_t events, void* data)
{
    struct epoll_event server_ev;

    server_ev.events   = events;
    server_ev.data.ptr = data;

    if(-1 == epoll_ctl(epollfd, op, fd, &server_ev)) {
        printf("Failed to add an event for socket%d Error:%s\n",
               fd, strerror(errno));
        exit(1);
    }

}

static int stat;
static int last_stat;

static void event_handle(void* ptr)
{
    event_t  *ev = ptr;
    uint32_t *len = (uint32_t *)ev->data;

    if(EPOLLIN == ev->event) {
        uint32_t buffer[1400];
        int      n  = 0;
        uint32_t blen;

        if (ev->length < sizeof(uint32_t)) {
            n = read(ev->fd, ev->data + ev->length, sizeof(uint32_t));
        } else {
            uint32_t blen = be32toh(*len);

            if (blen > 0) {
                n = read(ev->fd, ev->data + ev->length,
                         blen - (ev->length - sizeof(uint32_t)));
            } else {
                ev->length = 0;
                return;
            }
        }

        if(n == 0) {
            /*
             * Client closed connection.
             */
            printf("\nClient closed connection.\n");
            close(ev->fd);
            free(ev);
        } else
        if(n < 0) {
            perror("read from socket");
            close(ev->fd);
            free(ev);
        } else {
            int now = time(NULL);

            blen = be32toh(*len);
            ev->length += n;

            if (ev->length >= sizeof(uint32_t)
            &&  blen == ev->length - sizeof(uint32_t))
            {
                /* data complete */
                stat += ev->length;
                ev->length = 0;
            }

            if (now - last_stat >= 1) {
                printf("received %f MB.\n", ((float)stat / 1024 / 1024));
                last_stat = now;
                stat      = 0;
            }

            /*
             * We have read the data. Add an write event so that we can
             * write data whenever the socket is ready to be written.
             */
            /*
            printf("\nAdding write event.\n");
            event_set(epollfd, EPOLL_CTL_ADD, ev->fd, EPOLLOUT, ev);
            */
            event_set(epollfd, EPOLL_CTL_ADD, ev->fd, EPOLLIN, ev);
        }
    } else
    if(EPOLLOUT == ev->event) {
        int ret;

        printf("EPOLLOUT not handled yet \n");
        return;

        ret = write(ev->fd, (ev->data) + (ev->offset), ev->length);

        if( (ret < 0 && EINTR == errno) || ret < ev->length) {
            /*
             * We either got EINTR or write only sent partial data.
             * Add an write event. We still need to write data.
             */

            event_set(epollfd, EPOLL_CTL_ADD, ev->fd, EPOLLOUT, ev);

            if(-1 != ret) {
               /*
               * The previous write wrote only partial data to the socket.
               */
                ev->length = ev->length - ret;
                ev->offset = ev->offset + ret;
            }
        } else
        if(ret < 0) {
            /*
             * Some other error occured.
             */
            close(ev->fd);
            free(ev);
            return;
        } else {

          /*
           * The entire data was written. Add an read event,
           * to read more data from the socket.
           */
            printf("\nAdding Read Event.\n");
            event_set(epollfd, EPOLL_CTL_ADD, ev->fd, EPOLLIN, ev);
        }
    }
}


static void socket_set_non_blocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, NULL);

    if (flags < 0) {
        printf("fcntl F_GETFL failed.%s", strerror(errno));
        exit(1);
    }

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) < 0) {
        printf("fcntl F_SETFL failed.%s", strerror(errno));
        exit(1);
    }
}


int main(int argc, char** argv)
{
    int                 serverfd;
    int                 len    = 0;
    struct sockaddr_un  local;
    struct sockaddr_un  remote;
    struct epoll_event *events = NULL;

   /*
    * Create server socket. Specify the nonblocking socket option.
    *
    */
    serverfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if(serverfd < 0)
    {
        printf("Failed to create socket.%s\n", strerror(errno));
        exit(1);
    }

    bzero(&local, sizeof(local));

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);

   /*
    * Bind the server socket to the required ip-address and port.
    *
    */
    if(bind(serverfd, (struct sockaddr*)&local, len) < 0)
    {
        printf("Failed to bind.%s\n", strerror(errno));
        close(serverfd);
        exit(1);
    }

   /*
    * Mark the server socket has a socket that will be used to .
    * accept incoming connections.
    */
    if(listen(serverfd, MAXCONN) < 0)
    {
        printf("Failed to listen.%s", strerror(errno));
        exit(1);
    }

   /*
    * Create epoll context.
    */
    epollfd = epoll_create(MAXCONN);

    if(epollfd < 0)
    {
        printf("Failed to create epoll context.%s\n", strerror(errno));
        exit(1);
    }

   /*
    * Create read event for server socket.
    */
   event_set(epollfd, EPOLL_CTL_ADD, serverfd, EPOLLIN, &serverfd);

   /*
    * Main loop that listens for event.
    */
   events = calloc(MAXEVENTS, sizeof(struct epoll_event));

   while(1) {
       int n = epoll_wait(epollfd, events, MAXEVENTS, -1);

       if(n < 0) {
           printf("Failed to wait.%s\n", strerror(errno));
           exit(1);
       }

       for(int i = 0; i < n; i++) {
           if(events[i].data.ptr == &serverfd) {
               int connfd;

               if(events[i].events & EPOLLHUP || events[i].events & EPOLLERR) {
                   /*
                    * EPOLLHUP and EPOLLERR are always monitored.
                    */
                   close(serverfd);
                   exit(1);
               }

               /*
                * New client connection is available. Call accept.
                * Make connection socket non blocking.
                * Add read event for the connection socket.
                */
               len = sizeof(remote);
               connfd = accept(serverfd, (struct sockaddr*)&remote, &len);

               if(-1 == connfd) {
                   printf("Accept failed.%s\n", strerror(errno));
                   close(serverfd);
                   exit(1);
               } else {
                   event_t *ev = NULL;

                   socket_set_non_blocking(connfd);

                   printf("Adding a read event\n");
                   ev     = calloc(1, sizeof(event_t));
                   ev->fd = connfd;

                   /*
                    * Add a read event.
                    */
                   event_set(epollfd, EPOLL_CTL_ADD, ev->fd, EPOLLIN, ev);
               }
           } else {
               /*
                *A event has happend for one of the connection sockets.
                *Remove the connection socket from the epoll context.
                * When the event is handled by event_handle() function,
                *it will add the required event to listen for this
                *connection socket again to epoll
                *context
                */

                if(events[i].events & EPOLLHUP || events[i].events & EPOLLERR) {
                    event_t *ev = (event_t *) events[i].data.ptr;

                    printf("\nClosing connection socket\n");
                    close(ev->fd);
                    free(ev);
                } else
                if(EPOLLIN == events[i].events) {
                   event_t* ev = (event_t *) events[i].data.ptr;

                   ev->event = EPOLLIN;
                   /*
                    * Delete the read event.
                    */
                   event_set(epollfd, EPOLL_CTL_DEL, ev->fd, 0, 0);
                   event_handle(ev);
               }
               else if(EPOLLOUT == events[i].events)
               {
                   event_t* ev = (event_t*) events[i].data.ptr;

                   ev->event = EPOLLOUT;

                   /*
                    * Delete the write event.
                    */
                   event_set(epollfd, EPOLL_CTL_DEL, ev->fd, 0, 0);
                   event_handle(ev);
               }
           }

       }
   }

    free(events);
    exit(0);
}
