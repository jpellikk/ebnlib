/*
 * Copyright (c) 2015 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#ifndef _EBNLIB_TYPES_H
#define _EBNLIB_TYPES_H

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#ifdef PTHREAD
#include <pthread.h>
#endif

typedef enum {
    data_type_timer = 1,
    data_type_connection = 2
} data_type_e;

struct timer_data_t {
	data_type_e data_type;
	int32_t timer_fd;
	network_timer_type_e timer_type;
	user_data_t user_data;
};

struct connection_data_t {
	data_type_e data_type;
	int32_t socket_fd;
	int32_t socktype;
	connection_mode_e mode;
	user_data_t user_data;
};

struct network_data_t {
	struct network_attr_t attr;
	struct connection_data_t *ipc;
	int32_t loop_retval;
	int32_t epoll_fd;
#ifdef PTHREAD
	pthread_t thread;
#endif
};

#endif /* _EBNLIB_TYPES_H */
