/*
 * Copyright (c) 2016 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#include "ebnlib.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

struct timer_data_t {
	uint8_t timer_id;
	network_timer_t timer;
	uint8_t num_expirations;
};

static struct timer_data_t timer1 = {0};
static struct timer_data_t timer2 = {0};
static struct timer_data_t timer3 = {0};
static uint8_t num_expirations;
static network_t network;
static uint8_t running;

static void timer_callback(network_timer_t timer, const struct network_timer_event_t *event, user_data_t network_user_data);

static const struct network_attr_t network_attr = {
	.mode = network_mode_thread,
	.connection_event_cb = NULL,
	.timer_event_cb = timer_callback,
	.data_buffer = NULL,
	.buffer_len = 0,
	.user_data = {
		.ptr = NULL,
	},
};

static struct timespec time_spec1 = {
	.tv_nsec = 500000000,
	.tv_sec = 1,
};

static struct timespec time_spec2 = {
	.tv_nsec = 0,
	.tv_sec = 2,
};

int64_t format_nsec(int64_t s)
{
	s = (s + 500000) / 1000000;

	if (s == 1000) {
		--s;
	}

	return s;
}

static void timer_callback(network_timer_t timer, const struct network_timer_event_t *event, user_data_t network_user_data)
{
	(void)network_user_data;
	struct timer_data_t *data = (struct timer_data_t *)event->user_data.ptr;
	fprintf(stdout, "Timer%d expired: next_expiry=%d.%03ld, interval=%d.%03ld, num_expirations=%zu\n",
	        data->timer_id, (int)event->next_expiry->tv_sec, format_nsec(event->next_expiry->tv_nsec),
	        (int)event->interval->tv_sec, format_nsec(event->interval->tv_nsec), event->num_expirations);

	if (++data->num_expirations >= num_expirations) {
		if (network_timer_cancel(timer) == -1) {
			fprintf(stderr, "Canceling Timer%d failed.\n", data->timer_id);
		}

		if (data->timer_id == 1) {
			running = 0;
		}

		return;
	}

	if (data->timer_id == 2) {
		if (network_timer_start(timer, &time_spec1) == -1) {
			fprintf(stderr, "Starting Timer%d failed.\n", data->timer_id);
			return;
		}
	} else if (data->timer_id == 3) {
		struct timespec value;

		if (clock_gettime(CLOCK_REALTIME, &value) == -1) {
			perror("clock_gettime()");
			return;
		}

		value.tv_sec += time_spec1.tv_sec;
		value.tv_nsec += time_spec1.tv_nsec;

		if (value.tv_nsec > 999999999) {
			value.tv_nsec -= 1000000000;
			++value.tv_sec;
		}

		if (network_timer_start(timer, &value) == -1) {
			fprintf(stderr, "Starting Timer%d failed.\n", data->timer_id);
		}
	}
}

static int32_t create_network_timer1(void)
{
	const struct network_timer_attr_t timer_attr = {
		/* Timer with a constant interval */
		.type = network_timer_type_periodic,
		.network = &network,
		.user_data = {
			.ptr = &timer1,
		},
	};
	timer1.timer_id = 1;

	if (network_timer_create(&timer1.timer, &timer_attr) == -1) {
		fprintf(stderr, "Creating Timer1 failed!\n");
		return -1;
	}

	if (network_timer_start(timer1.timer, &time_spec2) == -1) {
		fprintf(stderr, "Starting Timer1 failed!\n");
		return -1;
	}

	return 0;
}

static int32_t create_network_timer2(void)
{
	const struct network_timer_attr_t timer_attr = {
		/* Timer with expiry time relative to now */
		.type = network_timer_type_relative,
		.network = &network,
		.user_data = {
			.ptr = &timer2,
		},
	};
	timer2.timer_id = 2;

	if (network_timer_create(&timer2.timer, &timer_attr) == -1) {
		fprintf(stderr, "Creating Timer2 failed!\n");
		return -1;
	}

	if (network_timer_start(timer2.timer, &time_spec1) == -1) {
		fprintf(stderr, "Starting Timer2 failed!\n");
		return -1;
	}

	return 0;
}

static int create_network_timer3(void)
{
	struct timespec value;
	const struct network_timer_attr_t timer_attr = {
		/* Timer with absolute expiry time */
		.type = network_timer_type_absolute,
		.network = &network,
		.user_data = {
			.ptr = &timer3,
		},
	};
	timer3.timer_id = 3;

	if (clock_gettime(CLOCK_REALTIME, &value) == -1) {
		perror("clock_gettime()");
		return -1;
	}

	if (network_timer_create(&timer3.timer, &timer_attr) == -1) {
		fprintf(stderr, "Creating Timer3 failed!\n");
		return -1;
	}

	value.tv_sec += time_spec1.tv_sec;
	value.tv_nsec += time_spec1.tv_nsec;

	if (value.tv_nsec > 999999999) {
		value.tv_nsec -= 1000000000;
		++value.tv_sec;
	}

	if (network_timer_start(timer3.timer, &value) == -1) {
		fprintf(stderr, "Starting Timer3 failed!\n");
		return -1;
	}

	return 0;
}

static void terminate(int32_t retval)
{
	if (timer1.timer) {
		network_timer_free(timer1.timer);
	}

	if (timer2.timer) {
		network_timer_free(timer2.timer);
	}

	if (timer3.timer) {
		network_timer_free(timer3.timer);
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
	num_expirations = 3;
	network = 0;
	running = 1;

	/* Create a network in the thread mode */
	if (network_create(&network, &network_attr) == -1) {
		terminate(EXIT_FAILURE);
	}

	if (create_network_timer1() == -1) {
		terminate(EXIT_FAILURE);
	}

	if (create_network_timer2() == -1) {
		terminate(EXIT_FAILURE);
	}

	if (create_network_timer3() == -1) {
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
