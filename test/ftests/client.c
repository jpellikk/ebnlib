/*
 * Copyright (c) 2015 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#include "ebnlib.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

static network_t network;
static connection_t connection;
static network_timer_t timer;
static uint64_t num_transmits;
static uint8_t buffer[1024];
static uint8_t running;

static void event_callback(connection_t connection, const struct connection_event_t *event, user_data_t network_user_data);
static void timer_callback(network_timer_t timer, const struct network_timer_event_t *event, user_data_t network_user_data);

static const struct network_attr_t network_attr = {
	.mode = network_mode_thread,
	.connection_event_cb = event_callback,
	.timer_event_cb = timer_callback,
	.data_buffer = buffer,
	.buffer_len = sizeof(buffer),
	.user_data = {
		.ptr = NULL,
	},
};

static const struct connection_attr_t connection_attr = {
	.network = &network,
	.hints = {
		.ai_family = AF_INET6,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = 0,
		.ai_protocol = IPPROTO_TCP,
		.ai_canonname = NULL,
		.ai_addrlen = 0,
		.ai_addr = NULL,
		.ai_next = NULL,
	},
	.mode = connection_mode_client,
	.hostname = "::1",
	.service = "12358",
	.src_addr = NULL,
	.src_addrlen = 0,
	.user_data = {
		.ptr = NULL,
	},
};

static const struct network_timer_attr_t timer_attr = {
	/* Timer with a constant interval */
	.type = network_timer_type_periodic,
	.network = &network,
	.user_data = {
		.ptr = NULL,
	},
};

static struct timespec time_spec = {
	/* Interval is 0.421 seconds */
	.tv_nsec = 421000000,
	.tv_sec = 0,
};

int64_t format_nsec(int64_t s)
{
	s = (s + 500000) / 1000000;

	if (s == 1000) {
		--s;
	}

	return s;
}

static void event_callback(connection_t connection, const struct connection_event_t *event, user_data_t network_user_data)
{
	(void)network_user_data;
	(void)connection;

	switch (event->event_type) {
		case connection_event_connection_created:
			fprintf(stdout, "Connection created.\n");
			network_timer_start(timer, &time_spec);
			break;

		case connection_event_data_received:
			fprintf(stdout, "Data received: length=%u, data=%s\n",
			        (unsigned)event->data_len, (char *)event->data_buffer);
			break;

		case connection_event_connection_closed:
			fprintf(stdout, "Connection closed.\n");
			running = 0; /* Terminate the program */
			break;

		case connection_event_connection_error:
			fprintf(stderr, "Connection failed.\n");
			running = 0; /* Terminate the program */
			break;

		case connection_event_connection_accepted:
		default:
			break;
	};
}

static void timer_callback(network_timer_t timer, const struct network_timer_event_t *event, user_data_t network_user_data)
{
	(void)network_user_data;
	fprintf(stdout, "Timer expired: next_expiry=%d.%03ld, interval=%d.%03ld, num_expirations=%zu\n",
	        (int)event->next_expiry->tv_sec, format_nsec(event->next_expiry->tv_nsec),
	        (int)event->interval->tv_sec, format_nsec(event->interval->tv_nsec),
	        event->num_expirations);

	if (num_transmits) {
		/* Send data to the server; ignore error */
		connection_send(connection, "Hello world!", 12);
	} else {
		network_timer_cancel(timer);
		connection_close(connection);
		running = 0;
		return;
	}

	num_transmits -= event->num_expirations;
}

static void terminate(int retval)
{
	if (timer) {
		network_timer_free(timer);
	}

	if (connection) {
		connection_close(connection);
		connection_free(connection);
	}

	if (network) {
		network_free(network);
	}

	fprintf(stdout, "Exit: %s\n",
	        retval == EXIT_SUCCESS
	        ? "Success" : "Failure");
	exit(retval);
}

int main(void)
{
	num_transmits = 3;
	connection = 0;
	network = 0;
	running = 1;
	timer = 0;

	/* Create a network in the thread mode */
	if (network_create(&network, &network_attr) == -1) {
		terminate(EXIT_FAILURE);
	}

	/* Create a periodical network timer */
	if (network_timer_create(&timer, &timer_attr) == -1) {
		terminate(EXIT_FAILURE);
	}

	/* Create a connection in the client mode */
	if (connection_create(&connection, &connection_attr) == -1) {
		terminate(EXIT_FAILURE);
	}

	/* Run the network in a separate thread */
	if (network_start(network) == -1) {
		terminate(EXIT_FAILURE);
	}

	while (running) {
		sleep(1);
	}

	/* Stop the network event loop */
	if (network_stop(network) == -1) {
		terminate(EXIT_FAILURE);
	}

	terminate(EXIT_SUCCESS);
	return 0;
}
