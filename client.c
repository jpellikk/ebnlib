#include "ebnlib.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

static network_t network;
static connection_t connection;
static uint8_t buffer[1024];
static uint8_t running;

static void event_callback(connection_t connection, const struct network_event_t *event);

static const struct network_attr_t network_attr = {
	.mode = network_mode_thread,
	.event_cb = event_callback,
	.data_buffer = buffer,
	.buffer_len = sizeof(buffer),
	.user_data = NULL,
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
};

static void event_callback(connection_t conn, const struct network_event_t *event)
{
	switch (event->event_type) {
		case network_event_connection_created:
			fprintf(stdout, "Connection created.\n");
			/* Send data to the server; ignore error */
			connection_send(conn, "Hello world!", 12);
			break;

		case network_event_data_received:
			fprintf(stdout, "Data received: length=%u, data=%s\n",
			        (unsigned)event->data_len, (char *)event->data_buffer);
			connection_close(conn);
			break;

		case network_event_connection_closed:
			fprintf(stdout, "Connection closed.\n");
			break;

		case network_event_connection_error:
			fprintf(stderr, "Connection failed.\n");
			break;

		case network_event_connection_accepted:
		default:
			break;
	};
}

static void signal_handler(int signal)
{
	(void)signal;
	running = 0;
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

	running = 1;

	/* Create a network in the thread mode */
	if (network_create(&network, &network_attr) == -1) {
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

	fprintf(stdout, "Press Ctrl+c to quit.\n");

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
