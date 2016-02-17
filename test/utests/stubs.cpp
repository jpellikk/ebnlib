/*
 * Copyright (c) 2016 Jani Pellikka <jpellikk@users.noreply.github.com>
 */
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <errno.h>

int pthread_create(pthread_t *__restrict __newthread, const pthread_attr_t *__restrict __attr, void * (*__start_routine)(void *), void *__restrict __arg) __THROWNL {
	(void)__newthread;
	(void)__attr;
	(void)__start_routine;
	(void)__arg;
	errno = EINVAL;
	return -1;
}

int pthread_join(pthread_t __th, void **__thread_return)
{
	(void)__th;
	(void)__thread_return;
	errno = EINVAL;
	return -1;
}

int epoll_create1(int __flags) __THROW {
	(void)__flags;
	errno = EINVAL;
	return -1;
}

int epoll_ctl(int __epfd, int __op, int __fd, struct epoll_event *__event) __THROW {
	(void)__epfd;
	(void)__op;
	(void)__fd;
	(void)__event;
	errno = EINVAL;
	return -1;
}

int timerfd_create(clockid_t __clock_id, int __flags) __THROW {
	(void)__clock_id;
	(void)__flags;
	errno = EINVAL;
	return -1;
}
