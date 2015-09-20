#include "network.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define _network ((struct network_data_t *)network)
#define _connection ((struct connection_data_t *)connection)

struct connection_data_t {
	connection_mode_e mode;
	int32_t socktype;
	int32_t socket;
};

struct network_data_t {
	struct network_attr_t attr;
	struct connection_data_t *ipc;
	int32_t epoll_fd;
	pthread_t thread;
};

static void *network_eventloop(void *args);
static int32_t network_socket_non_blocking(int32_t socket);
static int32_t network_ipc_create(struct network_data_t *network);
static int32_t network_socket_connect(int32_t socket, struct addrinfo *result);
static int32_t network_socket_bind(int32_t socket, struct addrinfo *result);
static int32_t network_socket_create(struct connection_data_t *connection,
                                     const struct connection_attr_t *attr);

int32_t network_create(network_t *network, const struct network_attr_t *attr)
{
	struct network_data_t *ptr;
	ptr = malloc(sizeof(*ptr));

	if (ptr == NULL) {
		perror("malloc()");
		return -1;
	}

	memset(ptr, 0, sizeof(*ptr));
	ptr->epoll_fd = epoll_create1(0);

	if (ptr->epoll_fd == -1) {
		perror("epoll_create1()");
		free(ptr);
		return -1;
	}

	ptr->attr = *attr;

	if (network_ipc_create(ptr) == -1) {
		return -1;
	}

	*network = (network_t)ptr;
	return 0;
}

int32_t network_free(network_t network)
{
	/* Clean the IPC resources */
	close(_network->ipc->socket);
	free(_network->ipc);
	/* Free the network resources */
	close(_network->epoll_fd);
	free(_network);
	return 0;
}

int32_t network_start(network_t network)
{
	if (_network->attr.mode == network_mode_thread) {
		if (pthread_create(&_network->thread, NULL,
		                   network_eventloop, _network)) {
			perror("pthread_create()");
			return -1;
		}
	} else if (_network->attr.mode == network_mode_mainloop) {
		/* Blocks execution until interrupted */
		network_eventloop(_network);
	} else {
		fprintf(stderr, "Unsupported network mode.");
		return -1;
	}

	return 0;
}

int32_t network_stop(network_t network)
{
	uint64_t data = 1;
	/* Signal the network event loop to stop */
	write(_network->ipc->socket, &data, sizeof(data));

	if (_network->attr.mode == network_mode_mainloop) {
		return 0;
	}

	if (pthread_join(_network->thread, NULL)) {
		perror("pthread_join()");
		return -1;
	}

	return 0;
}

int32_t connection_create(connection_t *connection,
                          const struct connection_attr_t *attr)
{
	struct connection_data_t *ptr;
	struct network_data_t *network;
	struct epoll_event event = {0};
	ptr = malloc(sizeof(*ptr));

	if (ptr == NULL) {
		perror("malloc()");
		return -1;
	}

	memset(ptr, 0, sizeof(*ptr));

	if (network_socket_create(ptr, attr) == -1) {
		free(ptr);
		return -1;
	}

	event.events = EPOLLIN | EPOLLET;

	if (attr->mode == connection_mode_client) {
		event.events |= EPOLLOUT;
	}

	event.data.ptr = ptr;
	network = (struct network_data_t *)(*attr->network);

	if (epoll_ctl(network->epoll_fd, EPOLL_CTL_ADD,
	              ptr->socket, &event) == -1) {
		perror("epoll_ctl()");
		free(ptr);
		return -1;
	}

	ptr->mode = attr->mode;
	*connection = (connection_t)ptr;
	return 0;
}

int32_t connection_free(connection_t connection)
{
	free(_connection);
	return 0;
}

int32_t connection_close(connection_t connection)
{
	return close(_connection->socket);
}

ssize_t connection_send(connection_t connection, const void *data, size_t len)
{
	ssize_t s = send(_connection->socket, data, len, 0);

	if (s == -1) {
		perror("send()");
		return -1;
	}

	return s;
}

ssize_t connection_sendto(connection_t connection, const void *data, size_t len,
                          const struct sockaddr *dest_addr, socklen_t addrlen)
{
	ssize_t s = sendto(_connection->socket, data, len, 0, dest_addr, addrlen);

	if (s == -1) {
		perror("sendto()");
		return -1;
	}

	return s;
}

static int32_t network_ipc_create(struct network_data_t *network)
{
	struct connection_data_t *conn;
	struct epoll_event event = {0};
	conn = malloc(sizeof(*conn));

	if (conn == NULL) {
		perror("malloc()");
		return -1;
	}

	memset(conn, 0, sizeof(*conn));
	conn->socket = eventfd(0, EFD_NONBLOCK);

	if (conn->socket == -1) {
		perror("eventfd()");
		free(conn);
		return -1;
	}

	event.data.ptr = conn;
	event.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(network->epoll_fd, EPOLL_CTL_ADD,
	              conn->socket, &event) == -1) {
		perror("epoll_ctl()");
		free(conn);
		return -1;
	}

	network->ipc = conn;
	return 0;
}

static int32_t network_socket_create(struct connection_data_t *connection, const struct connection_attr_t *attr)
{
	struct addrinfo *rp, *result;
	int32_t s = getaddrinfo(attr->hostname, attr->service, &attr->hints, &result);

	if (s != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		connection->socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

		if (connection->socket == -1) {
			continue;
		}

		if (network_socket_non_blocking(connection->socket) == -1) {
			close(connection->socket);
			continue;
		}

		if (attr->mode == connection_mode_client) {
			s = network_socket_connect(connection->socket, result);
		} else if (attr->mode == connection_mode_server) {
			s = network_socket_bind(connection->socket, result);
		} else {
			fprintf(stderr, "Unsupported connection mode.");
			s = -1;
		}

		if (s == 0) {
			connection->socktype = rp->ai_socktype;
			break;
		}

		close(connection->socket);
	}

	if (rp == NULL) {
		fprintf(stderr, "Creating socket failed.\n");
		freeaddrinfo(result);
		return -1;
	}

	freeaddrinfo(result);
	return 0;
}

static int32_t network_socket_connect(int32_t socket, struct addrinfo *result)
{
	/* TODO: add possibility to bind to a source port */
	if (connect(socket, result->ai_addr, result->ai_addrlen) == -1) {
		if (errno != EINPROGRESS) {
			perror("connect()");
			return -1;
		}
	}

	return 0;
}

static int32_t network_socket_bind(int32_t socket, struct addrinfo *result)
{
	if (bind(socket, result->ai_addr, result->ai_addrlen) == -1) {
		perror("bind()");
		return -1;
	}

	/* Listen only STREAM and SEQPACKET sockets */
	if (result->ai_socktype != SOCK_STREAM &&
	    result->ai_socktype != SOCK_SEQPACKET) {
		return 0;
	}

	if (listen(socket, SOMAXCONN) == -1) {
		perror("listen()");
		return -1;
	}

	return 0;
}

static int32_t network_socket_non_blocking(int32_t socket)
{
	int32_t flags = fcntl(socket, F_GETFL, 0);

	if (flags == -1) {
		perror("fcntl()");
		return -1;
	}

	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror("fcntl()");
		return -1;
	}

	return 0;
}

static void *network_eventloop(void *args)
{
	struct epoll_event *events;
	struct epoll_event event = {0};
	struct network_data_t *network;
	struct network_event_t network_event = {0};
	int32_t running = 1;
	events = calloc(SOMAXCONN, sizeof(event));

	if (events == NULL) {
		perror("calloc()");
		return NULL;
	}

	network = (struct network_data_t *)args;
	network_event.data_buffer = network->attr.data_buffer;
	network_event.user_data = network->attr.user_data;

	while (running) {
		int32_t i, j = epoll_wait(network->epoll_fd, events, SOMAXCONN, -1);

		if (j == -1) {
			/* Error or interrupt occurred */
			if (errno != EINTR) {
				perror("epoll_wait()");
			}

			break;
		}

		for (i = 0; i < j; ++i) {
			struct connection_data_t *connection = events[i].data.ptr;

			/* Any activity on the IPC socket ends the event loop */
			if (connection->socket == network->ipc->socket) {
				running = 0;
				break;
			}

			if ((events[i].events & EPOLLERR) ||
			    (events[i].events & EPOLLHUP)) {
				/* Error occurred; close the connection */
				close(connection->socket);
				network_event.event_type = network_event_connection_error;
				network->attr.event_cb((connection_t)connection, &network_event);
			} else if (events[i].events & EPOLLOUT) {
				/* Outgoing connection succeeded */
				event.events = EPOLLIN | EPOLLET;
				event.data.ptr = connection;

				/* Remove EPOLLOUT so that we don't receive it again */
				if (epoll_ctl(network->epoll_fd, EPOLL_CTL_MOD,
				              connection->socket, &event) == -1) {
					perror("epoll_ctl()");
					continue;
				}

				network_event.event_type = network_event_connection_created;
				network->attr.event_cb((connection_t)connection, &network_event);
			} else if (events[i].events & EPOLLIN) {
				if (connection->mode == connection_mode_server &&
				    (connection->socktype == SOCK_STREAM ||
				     connection->socktype == SOCK_SEQPACKET)) {
					/* New connection on a connection-oriented socket */
					while (1) {
						struct connection_data_t *ptr;
						struct sockaddr_storage in_addr;
						socklen_t in_len = sizeof(in_addr);
						int32_t socket = accept(connection->socket,
						                        (struct sockaddr *)
						                        &in_addr, &in_len);

						if (socket == -1) {
							/* Closed by the user? */
							if (errno == EBADF) {
								break;
							}

							if (errno == EAGAIN || errno == EWOULDBLOCK) {
								break;
							}

							perror("accept()");
							break;
						}

						ptr = malloc(sizeof(*ptr));

						if (ptr == NULL) {
							perror("malloc()");
							break;
						}

						memset(ptr, 0, sizeof(*ptr));
						ptr->mode = connection_mode_client;
						ptr->socket = socket;

						if (network_socket_non_blocking(ptr->socket) == -1) {
							free(ptr);
							break;
						}

						event.events = EPOLLIN | EPOLLET;
						event.data.ptr = ptr;

						if (epoll_ctl(network->epoll_fd, EPOLL_CTL_ADD,
						              ptr->socket, &event) == -1) {
							perror("epoll_ctl()");
							free(ptr);
							break;
						}

						network_event.addr_len = in_len;
						network_event.addr = (struct sockaddr *)&in_addr;
						network_event.new_connection = (connection_t)ptr;
						network_event.event_type = network_event_connection_accepted;
						network->attr.event_cb((connection_t)connection, &network_event);
					}
				} else {
					/* Data from an existing connection */
					int32_t closed = 0;

					while (1) {
						struct sockaddr_storage in_addr;
						socklen_t in_len = sizeof(in_addr);
						/* Structure in_addr is ignored with connection-oriented sockets */
						ssize_t count = recvfrom(connection->socket, network->attr.data_buffer,
						                         network->attr.buffer_len, 0,
						                         (struct sockaddr *)&in_addr, &in_len);

						if (count == -1) {
							/* Closed by the user? */
							if (errno == EBADF) {
								closed = 1;
								break;
							}

							if (errno != EAGAIN) {
								perror("read()");
								closed = 1;
							}

							break;
						} else if (count == 0) {
							/* Closed by the remote */
							closed = 1;
							break;
						}

						network_event.data_len = count;
						network_event.addr_len = in_len;
						network_event.addr = (struct sockaddr *)&in_addr;
						network_event.event_type = network_event_data_received;
						network->attr.event_cb((network_t)connection, &network_event);
					}

					if (closed) {
						close(connection->socket);
						network_event.event_type = network_event_connection_closed;
						network->attr.event_cb((network_t)connection, &network_event);
					}
				}
			}
		}
	}

	free(events);
	return NULL;
}
