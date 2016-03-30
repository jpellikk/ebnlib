/*
 * Copyright (c) 2016 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTest/TestHarness.h"
#include "ebnlib.h"
#include "types.h"

TEST_GROUP(NetworkTests)
{
};

TEST(NetworkTests, Test1)
{
	int32_t retval;
	network_t network;
	network_attr_t attr;
	memset(&attr, 0, sizeof(network_attr_t));
	retval = network_create(&network, &attr);
	CHECK(retval == -1);
}

TEST(NetworkTests, Test2)
{
	int32_t retval;
	network_data_t data;
	memset(&data, 0, sizeof(data));
	network_t network = (network_t)&data;
	retval = network_start(network);
	CHECK(retval == -1);
	data.attr.mode = network_mode_thread;
	retval = network_start(network);
	CHECK(retval == -1);
	data.attr.mode = network_mode_mainloop;
	retval = network_start(network);
	CHECK(retval == -1);
}

TEST(NetworkTests, Test3)
{
	int32_t retval;
	network_data_t data;
	memset(&data, 0, sizeof(data));
	network_t network = (network_t)&data;
	connection_data_t ipc;
	memset(&ipc, 0, sizeof(ipc));
	data.ipc = &ipc;
	ipc.socket_fd = -1;
	retval = network_stop(network);
	CHECK(retval == -1);
	ipc.socket_fd = eventfd(0, 0);
	retval = network_stop(network);
	CHECK(retval == -1);
	data.attr.mode = network_mode_thread;
	retval = network_stop(network);
	CHECK(retval == -1);
	close(ipc.socket_fd);
}

TEST_GROUP(ConnectionTests)
{
};

TEST(ConnectionTests, Test1)
{
	int32_t retval;
	connection_t connection;
	connection_attr_t attr;
	network_data_t data;
	struct sockaddr_in6 sockaddr;
	memset(&attr, 0, sizeof(connection_attr_t));
	retval = connection_create(&connection, &attr);
	CHECK(retval == -1);
	attr.hints.ai_family = AF_UNSPEC;
	attr.hints.ai_socktype = SOCK_DGRAM;
	attr.hints.ai_flags = AI_PASSIVE;
	attr.hints.ai_protocol = IPPROTO_UDP;
	attr.hints.ai_addrlen = 0;
	strcpy(attr.hostname, "::");
	strcpy(attr.service, "21537");
	retval = connection_create(&connection, &attr);
	CHECK(retval == -1);
	attr.mode = connection_mode_server;
	memset(&data, 0, sizeof(data));
	network_t network = (network_t)&data;
	attr.network = &network;
	data.epoll_fd = -1;
	retval = connection_create(&connection, &attr);
	CHECK(retval == -1);
	memset(&sockaddr, 0, sizeof(sockaddr_in6));
	sockaddr.sin6_family = AF_INET6;
	sockaddr.sin6_port = 1;
	attr.mode = connection_mode_client;
	attr.hints.ai_family = AF_INET6;
	attr.hints.ai_flags = 0;
	attr.src_addrlen = sizeof(sockaddr_in6);
	attr.src_addr = (struct sockaddr *)&sockaddr;
	retval = connection_create(&connection, &attr);
	CHECK(retval == -1);
}

TEST(ConnectionTests, Test2)
{
	int32_t retval;
	connection_data_t data;
	msghdr msg;
	uint8_t buffer[5] = {0};
	memset(&data, 0, sizeof(data));
	connection_t connection = (connection_t)&data;
	memset(&msg, 0, sizeof(msg));
	data.socket_fd = -1;
	retval = connection_sendmsg(connection, &msg);
	CHECK(retval == -1);
	retval = connection_send(connection, buffer, sizeof(buffer));
	CHECK(retval == -1);
	sockaddr dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	socklen_t addrlen = 0;
	retval = connection_sendto(connection, buffer,
	                           sizeof(buffer), &dest_addr, addrlen);
	CHECK(retval == -1);
}

TEST(ConnectionTests, Test3)
{
	int32_t retval;
	connection_data_t data;
	memset(&data, 0, sizeof(data));
	connection_t connection = (connection_t)&data;
	data.socket_fd = -1;
	retval = connection_close(connection);
	CHECK(retval == -1);
}

TEST_GROUP(NetworkTimerTests)
{
};

TEST(NetworkTimerTests, Test1)
{
	int32_t retval;
	network_timer_t timer;
	network_timer_attr_t attr;
	memset(&attr, 0, sizeof(network_timer_attr_t));
	retval = network_timer_create(&timer, &attr);
	CHECK(retval == -1);
}

TEST(NetworkTimerTests, Test2)
{
	int32_t retval;
	timer_data_t data;
	timespec spec;
	memset(&data, 0, sizeof(data));
	memset(&spec, 0, sizeof(spec));
	network_timer_t timer = (network_timer_t)&data;
	retval = network_timer_start(timer, &spec);
	CHECK(retval == -1);
	data.timer_type = network_timer_type_relative;
	data.timer_fd = -1;
	retval = network_timer_start(timer, &spec);
	CHECK(retval == -1);
}

TEST(NetworkTimerTests, Test3)
{
	int32_t retval;
	timer_data_t data;
	memset(&data, 0, sizeof(data));
	network_timer_t timer = (network_timer_t)&data;
	retval = network_timer_cancel(timer);
	CHECK(retval == -1);
}

int main(int argc, char **argv)
{
	return CommandLineTestRunner::RunAllTests(argc, argv);
}
