#define _DEFAULT_SOURCE

int set_nonblocking(int fd)
{
    int flags, result;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        goto err;
    result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1)
        goto err;
    return 0;
err:
    perror("set_nonblocking");
    return -1;
}

int set_tcp_nodelay(int fd)
{
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
}

static int create_and_bind_unix(const char *sockpath)
{
    struct sockaddr_un addr;
    int fd;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path) - 1);
    unlink(sockpath);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind error");
        return -1;
    }
    return fd;
}

static int create_and_bind_tcp(const char *host, const char *port)
{
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE};
    struct addrinfo *result, *rp;
    int sfd;
    if (getaddrinfo(host, port, &hints, &result) != 0)
    {
        perror("getaddrinfo error");
        return -1;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;
        /* set SO_REUSEADDR so the socket will be reusable after process kill */
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                       &(int){1}, sizeof(int)) < 0)
            perror("SO_REUSEADDR");
        if ((bind(sfd, rp->ai_addr, rp->ai_addrlen)) == 0)
        {
            /* Succesful bind */
            break;
        }
        close(sfd);
    }
    if (rp == NULL)
    {
        perror("Could not bind");
        return -1;
    }
    freeaddrinfo(result);
    return sfd;
}

int create_and_bind(const char *host, const char *port, int socket_family)
{
    int fd;
    if (socket_family == UNIX)
        fd = create_and_bind_unix(host);
    else
        fd = create_and_bind_tcp(host, port);
    return fd;
}

/*
 * Create a non-blocking socket and make it listen on the specfied address and
 * port
 */
int make_listen(const char *host, const char *port, int socket_family)
{
    int sfd;
    if ((sfd = create_and_bind(host, port, socket_family)) == -1)
        abort();
    if ((set_nonblocking(sfd)) == -1)
        abort();
    // Set TCP_NODELAY only for TCP sockets
    if (socket_family == INET)
        set_tcp_nodelay(sfd);
    if ((listen(sfd, conf->tcp_backlog)) == -1)
    {
        perror("listen");
        abort();
    }
    return sfd;
}

int accept_connection(int serversock)
{
    int clientsock;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if ((clientsock = accept(serversock,
                             (struct sockaddr *)&addr, &addrlen)) < 0)
        return -1;
    set_nonblocking(clientsock);

    // Set TCP_NODELAY only for TCP sockets
    if (conf->socket_family == INET)
        set_tcp_nodelay(clientsock);
    char ip_buff[INET_ADDRSTRLEN + 1];
    if (inet_ntop(AF_INET, &addr.sin_addr,
                  ip_buff, sizeof(ip_buff)) == NULL)
    {
        close(clientsock);
        return -1;
    }
    return clientsock;
}

/* Send all bytes contained in buf, updating sent bytes counter */
ssize_t send_bytes(int fd, const unsigned char *buf, size_t len)
{
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t n = 0;
    while (total < len)
    {
        n = send(fd, buf + total, bytesleft, MSG_NOSIGNAL);
        if (n == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                goto err;
        }
        total += n;
        bytesleft -= n;
    }
    return total;
err:
    fprintf(stderr, "send(2) - error sending data: %s", strerror(errno));
    return -1;
}

/*
 * Receive a given number of bytes on the descriptor fd, storing the stream of
 * data into a 2 Mb capped buffer
 */
ssize_t recv_bytes(int fd, unsigned char *buf, size_t bufsize)
{
    ssize_t n = 0;
    ssize_t total = 0;
    while (total < (ssize_t)bufsize)
    {
        if ((n = recv(fd, buf, bufsize - total, 0)) < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else
                goto err;
        }
        if (n == 0)
            return 0;
        buf += n;
        total += n;
    }
    return total;
err:
    fprintf(stderr, "recv(2) - error reading data: %s", strerror(errno));
    return -1;
}

/******************************
 *         EPOLL APIS         *
 ******************************/

#define EVLOOP_INITIAL_SIZE 4

struct evloop *evloop_create(int max_events, int timeout)
{
    struct evloop *loop = malloc(sizeof(*loop));
    evloop_init(loop, max_events, timeout);
    return loop;
}

void evloop_init(struct evloop *loop, int max_events, int timeout)
{
    loop->max_events = max_events;
    loop->events = malloc(sizeof(struct epoll_event) * max_events);
    loop->epollfd = epoll_create1(0);
    loop->timeout = timeout;
    loop->periodic_maxsize = EVLOOP_INITIAL_SIZE;
    loop->periodic_nr = 0;
    loop->periodic_tasks =
        malloc(EVLOOP_INITIAL_SIZE * sizeof(*loop->periodic_tasks));
    loop->status = 0;
}

void evloop_free(struct evloop *loop)
{
    free(loop->events);
    for (int i = 0; i < loop->periodic_nr; i++)
        free(loop->periodic_tasks[i]);
    free(loop->periodic_tasks);
    free(loop);
}

int epoll_add(int efd, int fd, int evs, void *data)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    // Being ev.data a union, in case of data != NULL, fd will be set to random
    if (data)
        ev.data.ptr = data;
    ev.events = evs | EPOLLET | EPOLLONESHOT;
    return epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_mod(int efd, int fd, int evs, void *data)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    // Being ev.data a union, in case of data != NULL, fd will be set to random
    if (data)
        ev.data.ptr = data;
    ev.events = evs | EPOLLET | EPOLLONESHOT;
    return epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
}

int epoll_del(int efd, int fd)
{
    return epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
}

void evloop_add_callback(struct evloop *loop, struct closure *cb)
{
    if (epoll_add(loop->epollfd, cb->fd, EPOLLIN, cb) < 0)
        perror("Epoll register callback: ");
}

void evloop_add_periodic_task(struct evloop *loop,
                              int seconds,
                              unsigned long long ns,
                              struct closure *cb)
{
    struct itimerspec timervalue;
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    memset(&timervalue, 0x00, sizeof(timervalue));
    // Set initial expire time and periodic interval
    timervalue.it_value.tv_sec = seconds;
    timervalue.it_value.tv_nsec = ns;
    timervalue.it_interval.tv_sec = seconds;
    timervalue.it_interval.tv_nsec = ns;
    if (timerfd_settime(timerfd, 0, &timervalue, NULL) < 0)
    {
        perror("timerfd_settime");
        return;
    }
    // Add the timer to the event loop
    struct epoll_event ev;
    ev.data.fd = timerfd;
    ev.events = EPOLLIN;
    if (epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, timerfd, &ev) < 0)
    {
        perror("epoll_ctl(2): EPOLLIN");
        return;
    }
    /* Store it into the event loop */
    if (loop->periodic_nr + 1 > loop->periodic_maxsize)
    {
        loop->periodic_maxsize *= 2;
        loop->periodic_tasks =
            realloc(loop->periodic_tasks,
                    loop->periodic_maxsize * sizeof(*loop->periodic_tasks));
    }
    loop->periodic_tasks[loop->periodic_nr] =
        malloc(sizeof(*loop->periodic_tasks[loop->periodic_nr]));
    loop->periodic_tasks[loop->periodic_nr]->closure = cb;
    loop->periodic_tasks[loop->periodic_nr]->timerfd = timerfd;
    loop->periodic_nr++;
}

int evloop_wait(struct evloop *el)
{
    int rc = 0;
    int events = 0;
    long int timer = 0L;
    int periodic_done = 0;
    while (1)
    {
        events = epoll_wait(el->epollfd, el->events,
                            el->max_events, el->timeout);
        if (events < 0)
        {
            if (errno == EINTR)
                continue;
            rc = -1;
            el->status = errno;
            break;
        }
        for (int i = 0; i < events; i++)
        {
            if ((el->events[i].events & EPOLLERR) ||
                (el->events[i].events & EPOLLHUP) ||
                (!(el->events[i].events & EPOLLIN) &&
                 !(el->events[i].events & EPOLLOUT)))
            {
                perror("epoll_wait(2)");
                shutdown(el->events[i].data.fd, 0);
                close(el->events[i].data.fd);
                el->status = errno;
                continue;
            }
            struct closure *closure = el->events[i].data.ptr;
            periodic_done = 0;
            for (int i = 0; i < el->periodic_nr && periodic_done == 0; i++)
            {
                if (el->events[i].data.fd == el->periodic_tasks[i]->timerfd)
                {
                    struct closure *c = el->periodic_tasks[i]->closure;
                    (void)read(el->events[i].data.fd, &timer, 8);
                    c->call(el, c->args);
                    periodic_done = 1;
                }
            }
            if (periodic_done == 1)
                continue;
            closure->call(el, closure->args);
        }
    }
    return rc;
}

int evloop_rearm_callback_read(struct evloop *el, struct closure *cb)
{
    return epoll_mod(el->epollfd, cb->fd, EPOLLIN, cb);
}

int evloop_rearm_callback_write(struct evloop *el, struct closure *cb)
{
    return epoll_mod(el->epollfd, cb->fd, EPOLLOUT, cb);
}

int evloop_del_callback(struct evloop *el, struct closure *cb)
{
    return epoll_del(el->epollfd, cb->fd);
}