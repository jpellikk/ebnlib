/*
 * Copyright (c) 2015 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#ifndef _EBNLIB_H
#define _EBNLIB_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <netdb.h>
#include <time.h>

typedef uintptr_t network_t;
typedef uintptr_t connection_t;
typedef uintptr_t network_timer_t;

typedef enum {
    connection_mode_client = 1,
    connection_mode_server = 2
} connection_mode_e;

typedef enum {
    network_mode_thread = 1,
    network_mode_mainloop = 2
} network_mode_e;

typedef enum {
    connection_event_data_received = 1,
    connection_event_connection_created = 2,
    connection_event_connection_accepted = 3,
    connection_event_connection_closed = 4,
    connection_event_connection_error = 5
} connection_event_e;

typedef enum {
    network_timer_type_periodic = 1,
    network_timer_type_relative = 2,
    network_timer_type_absolute = 3
} network_timer_type_e;

typedef union {
	void *ptr;
	uint32_t u32;
	uint64_t u64;
} user_data_t;

struct connection_event_t {
	connection_event_e event_type;
	connection_t new_connection;
	struct sockaddr *addr;
	socklen_t addr_len;
	void *data_buffer;
	size_t data_len;
	user_data_t user_data;
};

struct network_timer_event_t {
	struct timespec *next_expiry;
	struct timespec *interval;
	uint64_t num_expirations;
	user_data_t user_data;
};

struct connection_attr_t {
	network_t *network;
	struct addrinfo hints;
	connection_mode_e mode;
	char hostname[255];
	char service[32];
	socklen_t src_addrlen;
	struct sockaddr *src_addr;
	user_data_t user_data;
};

struct network_timer_attr_t {
	network_t *network;
	network_timer_type_e type;
	user_data_t user_data;
};

struct network_attr_t {
	void (*connection_event_cb)(connection_t connection,
	                            const struct connection_event_t *event,
	                            user_data_t network_user_data);
	void (*timer_event_cb)(network_timer_t timer,
	                       const struct network_timer_event_t *event,
	                       user_data_t network_user_data);
	network_mode_e mode;
	void *data_buffer;
	size_t buffer_len;
	user_data_t user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Network interface */
int32_t network_create(network_t *network, const struct network_attr_t *attr);
int32_t network_free(network_t network);
int32_t network_start(network_t network);
int32_t network_stop(network_t network);

/* Connection interface */
int32_t connection_create(connection_t *connection, const struct connection_attr_t *attr);
int32_t connection_free(connection_t connection);
int32_t connection_close(connection_t connection);
ssize_t connection_sendmsg(connection_t connection, const struct msghdr *msg);
ssize_t connection_send(connection_t connection, const void *data, size_t len);
ssize_t connection_sendto(connection_t connection, const void *data, size_t len,
                          const struct sockaddr *dest_addr, socklen_t addrlen);

/* Timer interface */
int32_t network_timer_create(network_timer_t *timer, const struct network_timer_attr_t *attr);
int32_t network_timer_free(network_timer_t timer);
int32_t network_timer_start(network_timer_t timer, const struct timespec *value);
int32_t network_timer_cancel(network_timer_t timer);

#ifdef __cplusplus
}
#endif

#endif /* _EBNLIB_H */
