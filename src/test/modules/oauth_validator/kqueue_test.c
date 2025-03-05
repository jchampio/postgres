#include "postgres.h"

#ifndef HAVE_SYS_EVENT_H

/* No-op. */
int main()
{
	return 0;
}

#else

#include <stdio.h>
#include <sys/event.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <libpq-fe.h>

static long start_us;

long
now_us()
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);

	return t.tv_sec * 1000 * 1000 + t.tv_nsec / 1000;
}

int
readable(int fd)
{
	int res = PQsocketPoll(fd, 1, 0, 0);

	printf("[%7ld] fd %d is %sreadable\n",
		   now_us() - start_us, fd, (res > 0) ? "" : "not ");
	return (res > 0);
}

int main()
{
	int one, two;
	struct kevent ev;

	one = kqueue();
	two = kqueue();

	start_us = now_us();

#define ADD_TO_MUX() \
	do \
	{ \
		EV_SET(&ev, two, EVFILT_READ, EV_ADD, 0, 0, 0); \
		if (kevent(one, &ev, 1, NULL, 0, NULL) < 0) \
		{ \
			perror("adding timer to mux"); \
			return 1; \
		} \
	} while (0)

#define REMOVE_FROM_MUX() \
	do \
	{ \
		EV_SET(&ev, two, EVFILT_READ, EV_DELETE, 0, 0, 0); \
		if (kevent(one, &ev, 1, NULL, 0, NULL) < 0) \
		{ \
			perror("removing timer from mux"); \
			return 1; \
		} \
	} while (0)

#define SET_TIMER() \
	do \
	{ \
		EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, 1, 0); \
		if (kevent(two, &ev, 1, NULL, 0, NULL) < 0) \
		{ \
			perror("setting EVFILT_TIMER"); \
			return 1; \
		} \
	} while (0)

#define STOP_TIMER() \
	do \
	{ \
		EV_SET(&ev, 1, EVFILT_TIMER, EV_DELETE, 0, 0, 0); \
		if (kevent(two, &ev, 1, NULL, 0, NULL) < 0) \
		{ \
			perror("deleting EVFILT_TIMER"); \
			return 1; \
		} \
	} while (0)

#define STATUS(STR) \
	do \
	{ \
		printf("[%7ld] " STR "\n", now_us() - start_us); \
		readable(one); \
		readable(two); \
	} while (0)

#define WAIT_READABLE() \
	do \
	{ \
		while (!readable(two)); \
		while (!readable(one)); \
		STATUS("queues are readable"); \
	} while (0)

	ADD_TO_MUX();
	STATUS("added timer to mux");

	SET_TIMER();
	STATUS("started timer");
	WAIT_READABLE();

	SET_TIMER();
	STATUS("retriggered timer");
	WAIT_READABLE();

	SET_TIMER();
	STATUS("retriggered timer");
	ADD_TO_MUX();
	STATUS("readded timer to mux");
	WAIT_READABLE();

	STOP_TIMER();
	STATUS("stopped timer");

	SET_TIMER();
	STATUS("restarted timer");
	WAIT_READABLE();

	REMOVE_FROM_MUX();
	ADD_TO_MUX();
	STATUS("removed/readded timer to mux");
	WAIT_READABLE();

	STOP_TIMER();
	STATUS("stopped timer");

	REMOVE_FROM_MUX();
	STATUS("removed timer from mux");

	ADD_TO_MUX();
	STATUS("readded timer to mux");
	SET_TIMER();
	STATUS("set timer");
	WAIT_READABLE();

	close(one);
	close(two);

	return 0;
}

#endif /* HAVE_SYS_EVENT_H */
