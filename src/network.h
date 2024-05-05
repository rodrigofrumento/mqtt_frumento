#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <util.h>

#define UNIX 0
#define INET 1

int set_nonblocking(int);
int set_tcp_nodelay(int);
int create_and_bind(const char *, const char *, int);
int make_listen(const char *, const char *, int);
int accept_connection(int);

ssize_t send_bytes(int, const unsigned char *, size_t);
ssize_t recv_bytes(int, unsigned char *, size_t);

struct evloop
{
    int epollfd;
    int max_events;
    int timeout;
    int status;
    struct epoll_event *events;
    /* Dynamic array of periodic tasks, a pair descriptor - closure */
    int periodic_maxsize;
    int periodic_nr;
    struct
    {
        int timerfd;
        struct closure *closure;
    } **periodic_tasks;
} evloop;

typedef void callback(struct evloop *, void *);

struct closure
{
    int fd;
    void *obj;
    void *args;
    char closure_id[UUID_LEN];
    struct bytestring *payload;
    callback *call;
};

struct evloop *evloop_create(int, int);
void evloop_init(struct evloop *, int, int);
void evloop_free(struct evloop *);

int evloop_wait(struct evloop *);

void evloop_add_callback(struct evloop *, struct closure *);

void evloop_add_periodic_task(struct evloop *,
                              int,
                              unsigned long long,
                              struct closure *);

int evloop_del_callback(struct evloop *, struct closure *);
int evloop_rearm_callback_read(struct evloop *, struct closure *);
int evloop_rearm_callback_write(struct evloop *, struct closure *);
int epoll_add(int, int, int, void *);
int epoll_mod(int, int, int, void *);
int epoll_del(int, int);

#endif