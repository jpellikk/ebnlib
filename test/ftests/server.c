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
static uint8_t buffer[1024];

static void event_callback(connection_t connection, const struct connection_event_t *event, user_data_t network_user_data);

static const struct network_attr_t network_attr = {
	.mode = network_mode_mainloop,
	.connection_event_cb = event_callback,
	.timer_event_cb = NULL,
	.data_buffer = buffer,
	.buffer_len = sizeof(buffer),
	.user_data = {
		.ptr = NULL,
	},
};

static const struct connection_attr_t connection_attr = {
	.network = &network,
	.hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
		.ai_protocol = IPPROTO_TCP,
		.ai_canonname = NULL,
		.ai_addrlen = 0,
		.ai_addr = NULL,
		.ai_next = NULL,
	},
	.mode = connection_mode_server,
	.hostname = "::",
	.service = "12358",
	.src_addr = NULL,
	.src_addrlen = 0,
	.user_data = {
		.ptr = NULL,
	},
};

static void event_callback(connection_t connection, const struct connection_event_t *event, user_data_t network_user_data)
{
	char host[NI_MAXHOST], service[NI_MAXSERV];
	(void)network_user_data;

	switch (event->event_type) {
		case connection_event_connection_accepted:
			if (getnameinfo(event->addr, event->addr_len,
			                host, sizeof(host), service, sizeof(service),
			                NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
				perror("getnameinfo()");
				break;
			}

			fprintf(stdout, "New connection: host=%s, port=%s\n", host, service);
			break;

		case connection_event_data_received:
			fprintf(stdout, "Data received: length=%u, data=%s\n",
			        (unsigned)event->data_len, (char *)event->data_buffer);
			/* Send back the received data */
			connection_send(connection, event->data_buffer, event->data_len);
			break;

		case connection_event_connection_closed:
			fprintf(stdout, "Connection closed.\n");
			/* Free connection resources */
			connection_free(connection);
			break;

		case connection_event_connection_error:
			fprintf(stderr, "Connection error.\n");
			/* Free connection resources */
			connection_free(connection);
			break;

		case connection_event_connection_created:
		default:
			break;
	};
}

static void signal_handler(int signal)
{
	(void)signal;
}

static void terminate(int retval)
{
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
	connection = 0;
	network = 0;

	/* Initialize the program */
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		perror("signal()");
		terminate(EXIT_FAILURE);
	}

	/* Create a network in the mainloop mode */
	if (network_create(&network, &network_attr) == -1) {
		terminate(EXIT_FAILURE);
	}

	/* Create a connection in the server mode */
	if (connection_create(&connection, &connection_attr) == -1) {
		terminate(EXIT_FAILURE);
	}

	fprintf(stdout, "Press Ctrl+c to quit.\n");

	/* Run the network in the main thread until
	 * a signal interrupts the event loop */
	if (network_start(network) == -1) {
		terminate(EXIT_FAILURE);
	}

	/* Stop the network event loop */
	if (network_stop(network) == -1) {
		terminate(EXIT_FAILURE);
	}

	terminate(EXIT_SUCCESS);
	return 0;
}
