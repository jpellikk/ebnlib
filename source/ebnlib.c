/*
 * Copyright (c) 2015 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#include "ebnlib.h"
#include "types.h"

#define _network ((struct network_data_t *)network)
#define _connection ((struct connection_data_t *)connection)
#define _timer ((struct timer_data_t *)timer)

#ifdef DEBUG
#define _fprintf(...) do { fprintf(__VA_ARGS__); } while(0)
#define _perror(x) do { perror((x)); } while(0)
#else
#define _fprintf(...) do { } while(0)
#define _perror(x) do { } while(0)
#endif

static void *network_eventloop(void *args);
static int32_t network_socket_non_blocking(int32_t socket_fd);
static int32_t network_ipc_create(struct network_data_t *network);
static int32_t network_socket_connect(int32_t socket_fd, struct addrinfo *result,
                                      const struct connection_attr_t *attr);
static int32_t network_socket_bind(int32_t socket_fd, struct addrinfo *result);
static void handle_timer(struct network_data_t *network, struct timer_data_t *timer);
static int32_t network_socket_create(struct connection_data_t *connection,
                                     const struct connection_attr_t *attr);

int32_t network_create(network_t *network, const struct network_attr_t *attr)
{
	struct network_data_t *ptr;
	ptr = malloc(sizeof(*ptr));

	if (ptr == NULL) {
		_perror("malloc()");
		return -1;
	}

	memset(ptr, 0, sizeof(*ptr));
	ptr->epoll_fd = epoll_create1(0);

	if (ptr->epoll_fd == -1) {
		_perror("epoll_create1()");
		free(ptr);
		return -1;
	}

	ptr->attr = *attr;

	if (network_ipc_create(ptr) == -1) {
		free(ptr);
		return -1;
	}

	*network = (network_t)ptr;
	return 0;
}

int32_t network_free(network_t network)
{
	/* Clean the IPC resources */
	close(_network->ipc->socket_fd);
	free(_network->ipc);
	/* Free the network resources */
	close(_network->epoll_fd);
	free(_network);
	return 0;
}

int32_t network_start(network_t network)
{
#ifdef PTHREAD

	if (_network->attr.mode == network_mode_thread) {
		if (pthread_create(&_network->thread, NULL,
		                   network_eventloop, _network)) {
			_perror("pthread_create()");
			return -1;
		}
	} else
#endif
		if (_network->attr.mode == network_mode_mainloop) {
			/* Blocks execution until interrupted */
			network_eventloop(_network);
			return _network->loop_retval;
		} else {
			_fprintf(stderr, "Invalid network mode: %d\n",
			         _network->attr.mode);
			return -1;
		}

	return 0;
}

int32_t network_stop(network_t network)
{
	uint64_t data = 1;

	/* Signal the network event loop to stop */
	if (write(_network->ipc->socket_fd, &data, sizeof(data)) == -1) {
		_perror("write()");
		return -1;
	}

#ifdef PTHREAD

	if (_network->attr.mode == network_mode_thread) {
		if (pthread_join(_network->thread, NULL)) {
			_perror("pthread_join()");
			return -1;
		}
	} else
#endif
		if (_network->attr.mode == network_mode_mainloop) {
			return 0;
		} else {
			_fprintf(stderr, "Invalid network mode: %d\n", _network->attr.mode);
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
		_perror("malloc()");
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
	              ptr->socket_fd, &event) == -1) {
		_perror("epoll_ctl()");
		close(ptr->socket_fd);
		free(ptr);
		return -1;
	}

	ptr->mode = attr->mode;
	ptr->user_data = attr->user_data;
	ptr->data_type = data_type_connection;
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
	return close(_connection->socket_fd);
}

ssize_t connection_sendmsg(connection_t connection, const struct msghdr *msg)
{
	ssize_t s = sendmsg(_connection->socket_fd, msg, 0);

	if (s == -1) {
		_perror("sendmsg()");
		return -1;
	}

	return s;
}

ssize_t connection_send(connection_t connection, const void *data, size_t len)
{
	ssize_t s = send(_connection->socket_fd, data, len, 0);

	if (s == -1) {
		_perror("send()");
		return -1;
	}

	return s;
}

ssize_t connection_sendto(connection_t connection, const void *data, size_t len,
                          const struct sockaddr *dest_addr, socklen_t addrlen)
{
	ssize_t s = sendto(_connection->socket_fd, data, len, 0, dest_addr, addrlen);

	if (s == -1) {
		_perror("sendto()");
		return -1;
	}

	return s;
}

int32_t network_timer_create(network_timer_t *timer, const struct network_timer_attr_t *attr)
{
	struct timer_data_t *ptr;
	struct network_data_t *network;
	struct epoll_event event = {0};
	ptr = malloc(sizeof(*ptr));

	if (ptr == NULL) {
		_perror("malloc()");
		return -1;
	}

	memset(ptr, 0, sizeof(*ptr));
	ptr->timer_fd = timerfd_create(attr->type == network_timer_type_absolute
	                               ? CLOCK_REALTIME : CLOCK_MONOTONIC, TFD_NONBLOCK);

	if (ptr->timer_fd == -1) {
		_perror("timerfd_create()");
		free(ptr);
		return -1;
	}

	event.events = EPOLLIN;
	event.data.ptr = ptr;
	network = (struct network_data_t *)(*attr->network);

	if (epoll_ctl(network->epoll_fd, EPOLL_CTL_ADD,
	              ptr->timer_fd, &event) == -1) {
		_perror("epoll_ctl()");
		close(ptr->timer_fd);
		free(ptr);
		return -1;
	}

	ptr->data_type = data_type_timer;
	ptr->user_data = attr->user_data;
	ptr->timer_type = attr->type;
	*timer = (network_timer_t)ptr;
	return 0;
}

int32_t network_timer_free(network_timer_t timer)
{
	/* Remove from the event list */
	close(_timer->timer_fd);
	/* Free resources */
	free(_timer);
	return 0;
}

int32_t network_timer_start(network_timer_t timer, const struct timespec *value)
{
	int32_t flags = 0;
	struct itimerspec spec;
	memset(&spec, 0, sizeof(spec));

	if (_timer->timer_type == network_timer_type_periodic) {
		/* Value specifies expiration interval */
		spec.it_interval = *value;
		spec.it_value = *value;
	} else if (_timer->timer_type == network_timer_type_relative) {
		/* Expiry time relative to now (expires only once) */
		spec.it_value = *value;
	} else if (_timer->timer_type == network_timer_type_absolute) {
		/* Expiry time as an absolute time (expires only once) */
		flags = TFD_TIMER_ABSTIME;
		spec.it_value = *value;
	} else {
		_fprintf(stderr, "Invalid timer type: %d\n", _timer->timer_type);
		return -1;
	}

	if (timerfd_settime(_timer->timer_fd, flags, &spec, NULL) == -1) {
		_perror("timerfd_settime()");
		return -1;
	}

	return 0;
}

int32_t network_timer_cancel(network_timer_t timer)
{
	struct itimerspec spec;
	memset(&spec, 0, sizeof(spec));

	if (timerfd_settime(_timer->timer_fd, 0, &spec, NULL) == -1) {
		_perror("timerfd_settime()");
		return -1;
	}

	return 0;
}

static int32_t network_ipc_create(struct network_data_t *network)
{
	struct connection_data_t *conn;
	struct epoll_event event = {0};
	conn = malloc(sizeof(*conn));

	if (conn == NULL) {
		_perror("malloc()");
		return -1;
	}

	memset(conn, 0, sizeof(*conn));
	conn->data_type = data_type_connection;
	conn->socket_fd = eventfd(0, EFD_NONBLOCK);

	if (conn->socket_fd == -1) {
		_perror("eventfd()");
		free(conn);
		return -1;
	}

	event.data.ptr = conn;
	event.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(network->epoll_fd, EPOLL_CTL_ADD,
	              conn->socket_fd, &event) == -1) {
		_perror("epoll_ctl()");
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
		_fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		connection->socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

		if (connection->socket_fd == -1) {
			continue;
		}

		if (network_socket_non_blocking(connection->socket_fd) == -1) {
			close(connection->socket_fd);
			continue;
		}

		if (attr->mode == connection_mode_client) {
			s = network_socket_connect(connection->socket_fd, rp, attr);
		} else if (attr->mode == connection_mode_server) {
			s = network_socket_bind(connection->socket_fd, rp);
		} else {
			_fprintf(stderr, "Invalid connection mode: %d\n", attr->mode);
			s = -1;
		}

		if (s == 0) {
			connection->socktype = rp->ai_socktype;
			break;
		}

		close(connection->socket_fd);
	}

	if (rp == NULL) {
		_fprintf(stderr, "Creating socket failed.\n");
		freeaddrinfo(result);
		return -1;
	}

	freeaddrinfo(result);
	return 0;
}

static int32_t network_socket_connect(int32_t socket_fd, struct addrinfo *result, const struct connection_attr_t *attr)
{
	if (attr->src_addrlen > 0) {
		/* Bind to a source address/port if given by the user */
		if (bind(socket_fd, attr->src_addr, attr->src_addrlen) == -1) {
			_perror("bind()");
			return -1;
		}
	}

	if (connect(socket_fd, result->ai_addr, result->ai_addrlen) == -1) {
		if (errno != EINPROGRESS) {
			_perror("connect()");
			return -1;
		}
	}

	return 0;
}

static int32_t network_socket_bind(int32_t socket_fd, struct addrinfo *result)
{
	if (bind(socket_fd, result->ai_addr, result->ai_addrlen) == -1) {
		_perror("bind()");
		return -1;
	}

	/* Listen only STREAM and SEQPACKET sockets */
	if (result->ai_socktype != SOCK_STREAM &&
	    result->ai_socktype != SOCK_SEQPACKET) {
		return 0;
	}

	if (listen(socket_fd, SOMAXCONN) == -1) {
		_perror("listen()");
		return -1;
	}

	return 0;
}

static int32_t network_socket_non_blocking(int32_t socket_fd)
{
	int32_t flags = fcntl(socket_fd, F_GETFL, 0);

	if (flags == -1) {
		_perror("fcntl()");
		return -1;
	}

	if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		_perror("fcntl()");
		return -1;
	}

	return 0;
}

static void *network_eventloop(void *args)
{
	struct epoll_event *events, event = {0};
	struct connection_event_t conn_event = {0};
	struct network_data_t *network;
	events = calloc(SOMAXCONN, sizeof(event));
	network = (struct network_data_t *)args;
	network->loop_retval = 0;

	if (events == NULL) {
		network->loop_retval = -1;
		_perror("calloc()");
		return NULL;
	}

	conn_event.data_buffer = network->attr.data_buffer;

	while (1) {
		int32_t i, j = epoll_wait(network->epoll_fd, events, SOMAXCONN, -1);

		if (j == -1) {
			/* Error or interrupt occurred */
			if (errno != EINTR) {
				network->loop_retval = -1;
				_perror("epoll_wait()");
			}

			break;
		}

		for (i = 0; i < j; ++i) {
			struct connection_data_t *connection = events[i].data.ptr;

			/* Any activity on the IPC socket ends the event loop */
			if (connection->socket_fd == network->ipc->socket_fd) {
				goto END;
			}

			if ((events[i].events & EPOLLERR) ||
			    (events[i].events & EPOLLHUP)) {
				/* Error occurred; close the connection */
				close(connection->socket_fd);
				conn_event.user_data = connection->user_data;
				conn_event.data_len = conn_event.addr_len = 0;
				conn_event.event_type = connection_event_connection_error;
				network->attr.connection_event_cb((connection_t)connection,
				                                  &conn_event, network->attr.user_data);
			} else if (events[i].events & EPOLLOUT) {
				/* Outgoing connection succeeded */
				event.events = EPOLLIN | EPOLLET;
				event.data.ptr = connection;

				/* Remove EPOLLOUT so that we don't receive it again */
				if (epoll_ctl(network->epoll_fd, EPOLL_CTL_MOD,
				              connection->socket_fd, &event) == -1) {
					_perror("epoll_ctl()");
					continue;
				}

				conn_event.data_len = conn_event.addr_len = 0;
				conn_event.user_data = connection->user_data;
				conn_event.event_type = connection_event_connection_created;
				network->attr.connection_event_cb((connection_t)connection,
				                                  &conn_event, network->attr.user_data);
			} else if (events[i].events & EPOLLIN) {
				/* Handle timer expiration event if data type is timer */
				if (connection->data_type == data_type_timer) {
					handle_timer(network, (struct timer_data_t *)connection);
					continue;
				}

				if (connection->mode == connection_mode_server &&
				    (connection->socktype == SOCK_STREAM ||
				     connection->socktype == SOCK_SEQPACKET)) {
					/* New connection on a connection-oriented socket */
					while (1) {
						struct connection_data_t *ptr;
						struct sockaddr_storage in_addr;
						socklen_t in_len = sizeof(in_addr);
						int32_t socket_fd = accept(connection->socket_fd,
						                           (struct sockaddr *)
						                           &in_addr, &in_len);

						if (socket_fd == -1) {
							/* Closed by the user? */
							if (errno == EBADF) {
								break;
							}

							/* No more incoming connections; break the loop */
							if (errno == EAGAIN || errno == EWOULDBLOCK) {
								break;
							}

							_perror("accept()");
							break;
						}

						ptr = malloc(sizeof(*ptr));

						if (ptr == NULL) {
							_perror("malloc()");
							close(socket_fd);
							break;
						}

						memset(ptr, 0, sizeof(*ptr));
						ptr->mode = connection_mode_client;
						ptr->data_type = data_type_connection;
						ptr->socket_fd = socket_fd;

						if (network_socket_non_blocking(ptr->socket_fd) == -1) {
							close(socket_fd);
							free(ptr);
							break;
						}

						event.events = EPOLLIN | EPOLLET;
						event.data.ptr = ptr;

						if (epoll_ctl(network->epoll_fd, EPOLL_CTL_ADD,
						              ptr->socket_fd, &event) == -1) {
							_perror("epoll_ctl()");
							close(socket_fd);
							free(ptr);
							break;
						}

						conn_event.data_len = 0;
						conn_event.addr_len = in_len;
						conn_event.addr = (struct sockaddr *)&in_addr;
						conn_event.new_connection = (connection_t)ptr;
						conn_event.user_data = connection->user_data;
						conn_event.event_type = connection_event_connection_accepted;
						network->attr.connection_event_cb((connection_t)connection,
						                                  &conn_event, network->attr.user_data);
					}
				} else {
					/* Data from an existing connection */
					int32_t closed = 0;

					while (1) {
						struct sockaddr_storage in_addr;
						socklen_t in_len = sizeof(in_addr);
						/* Structure in_addr is ignored with connection-oriented sockets */
						ssize_t count = (connection->socktype == SOCK_SEQPACKET) ?
						                recvmsg(connection->socket_fd, network->attr.data_buffer, 0) :
						                recvfrom(connection->socket_fd, network->attr.data_buffer,
						                         network->attr.buffer_len, 0, (struct sockaddr *)
						                         &in_addr, &in_len);

						if (count == -1) {
							/* Closed by the user? */
							if (errno == EBADF) {
								closed = 1;
								break;
							}

							/* No more data to read; break the loop */
							if (errno == EAGAIN || errno == EWOULDBLOCK) {
								break;
							}

							_perror("recv()");
							closed = 1;
							break;
						} else if (count == 0) {
							/* Closed by the remote host */
							closed = 1;
							break;
						}

						conn_event.data_len = count;
						conn_event.addr_len = in_len;
						conn_event.addr = (struct sockaddr *)&in_addr;
						conn_event.user_data = connection->user_data;
						conn_event.event_type = connection_event_data_received;
						network->attr.connection_event_cb((connection_t)connection,
						                                  &conn_event, network->attr.user_data);
					}

					if (closed) {
						close(connection->socket_fd);
						conn_event.data_len = conn_event.addr_len = 0;
						conn_event.user_data = connection->user_data;
						conn_event.event_type = connection_event_connection_closed;
						network->attr.connection_event_cb((connection_t)connection,
						                                  &conn_event, network->attr.user_data);
					}
				}
			}
		}
	}

END:
	free(events);
	return NULL;
}

static void handle_timer(struct network_data_t *network, struct timer_data_t *timer)
{
	struct network_timer_event_t timer_event = {0};
	struct itimerspec timer_spec;
	uint64_t exp;
	ssize_t bytes;
	bytes = read(timer->timer_fd, &exp, sizeof(uint64_t));

	if (bytes == -1) {
		/* Closed by the user? */
		if (errno == EBADF) {
			return;
		}

		/* No data available for reading; return */
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return;
		}

		_perror("read()");
		close(timer->timer_fd);
		return;
	}

	if (bytes != sizeof(uint64_t)) {
		_fprintf(stderr, "Invalid number of timer bytes: %ld\n", bytes);
		return;
	}

	timerfd_gettime(timer->timer_fd, &timer_spec);
	timer_event.num_expirations = exp;
	timer_event.user_data = timer->user_data;
	timer_event.next_expiry = &timer_spec.it_value;
	timer_event.interval = &timer_spec.it_interval;
	network->attr.timer_event_cb((network_timer_t)timer,
	                             &timer_event, network->attr.user_data);
}
