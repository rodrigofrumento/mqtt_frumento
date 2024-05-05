#ifndef SERVER_H
#define SERVER_H

/*
 * Epoll default settings for concurrent events monitored and timeout, -1
 * means no timeout at all, blocking indefinitely
 */
#define EPOLL_MAX_EVENTS 256
#define EPOLL_TIMEOUT -1

/* Error codes for packet reception, signaling respectively
 * - client disconnection
 * - error reading packet
 * - error packet sent exceeds size defined by configuration (generally default
 *   to 2MB)
 */
#define ERRCLIENTDC 1
#define ERRPACKETERR 2
#define ERRMAXREQSIZE 3

/* Return code of handler functions, signaling if there's data payload to be
 * sent out or if the server just need to re-arm closure for reading incoming
 * bytes
 */
#define REARM_R 0
#define REARM_W 1

int start_server(const char *, const char *);

struct sol_info
{
    /* Number of clients currently connected */
    int nclients;
    /* Total number of clients connected since the start */
    int nconnections;
    /* Timestamp of the start time */
    long long start_time;
    /* Total number of bytes received */
    long long bytes_recv;
    /* Total number of bytes sent out */
    long long bytes_sent;
    /* Total number of sent messages */
    long long messages_sent;
    /* Total number of received messages */
    long long messages_recv;
};

#endif