#ifndef LIBSUIW_INTERNAL_H
#define LIBSUIW_INTERNAL_H

const char *daemon_hostname = "127.0.0.1";
const char *daemon_port = "9999";

/* Library <-> Daemon messages */

enum daemon_msg_t {
    // Request daemon allocate a connection management event channel.
    DAEMON_CREATE_EC,
    // Request daemon allocate a completion queue.
    DAEMON_CREATE_CQ,
    // Request daemon allocate a queue pair.
    DAEMON_CREATE_QP,
};

struct __attribute__((packed)) daemon_msg {
    // Indicates the type of message.
    daemon_msg_t type;
};

#endif // LIBSUIW_INTERNAL_H
