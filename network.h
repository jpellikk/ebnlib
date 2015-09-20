#ifndef _NETWORK_H
#define _NETWORK_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <netdb.h>

typedef uintptr_t network_t;
typedef uintptr_t connection_t;

typedef enum {
    connection_mode_client = 1,
    connection_mode_server = 2
} connection_mode_e;

typedef enum {
    network_mode_thread = 1,
    network_mode_mainloop = 2
} network_mode_e;

typedef enum {
    network_event_data_received = 1,
    network_event_connection_created = 2,
    network_event_connection_accepted = 3,
    network_event_connection_closed = 4,
    network_event_connection_error = 5
} network_event_e;

struct network_event_t {
	network_event_e event_type;
	connection_t new_connection;
	struct sockaddr *addr;
	socklen_t addr_len;
	void *data_buffer;
	size_t data_len;
	void *user_data;
};

struct connection_attr_t {
	network_t *network;
	struct addrinfo hints;
	connection_mode_e mode;
	char hostname[255];
	char service[32];
};

struct network_attr_t {
	void (*event_cb)(connection_t connection,
	                 const struct network_event_t *event);
	network_mode_e mode;
	void *data_buffer;
	size_t buffer_len;
	void *user_data;
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
ssize_t connection_send(connection_t connection, const void *data, size_t len);
ssize_t connection_sendto(connection_t connection, const void *data, size_t len,
                          const struct sockaddr *dest_addr, socklen_t addrlen);

#ifdef __cplusplus
}
#endif

#endif /* _NETWORK_H */
