#include "postgres.h"

#ifndef HAVE_SYS_EVENT_H

/* No-op. */
int
main()
{
	return 0;
}

#else

#include <limits.h>
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

	clock_gettime(CLOCK_REALTIME, &t);

	return t.tv_sec * 1000 * 1000 + t.tv_nsec / 1000;
}

int
main()
{
	int			kq;
	struct kevent ev;
	int			timeout;
	int			res;

	kq = kqueue();
	start_us = now_us();

	/* Set the timer (1 ms timeout). */
	timeout = 1;
	EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, timeout, 0);
	if (kevent(kq, &ev, 1, NULL, 0, NULL) < 0)
	{
		perror("setting EVFILT_TIMER");
		return 1;
	}

	printf("[%7ld us] timer is set\n", now_us() - start_us);

	/* Wait (up to a second) for readable. */
	res = PQsocketPoll(kq, 1, 0, now_us() + 1000 * 1000);
	if (res == -1)
	{
		perror("polling kqueue");
		return 1;
	}

	printf("[%7ld us] kqueue is %sreadable\n",
		   now_us() - start_us, (res > 0) ? "" : "not ");

	/* Reset the timer far in the future. */
	timeout = INT_MAX;
	EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, timeout, 0);
	if (kevent(kq, &ev, 1, NULL, 0, NULL) < 0)
	{
		perror("setting EVFILT_TIMER");
		return 1;
	}

	printf("[%7ld us] timer is reset\n", now_us() - start_us);

	/* Check for readable. */
	res = PQsocketPoll(kq, 1, 0, 0);
	if (res == -1)
	{
		perror("polling kqueue");
		return 1;
	}

	printf("[%7ld us] kqueue is %sreadable\n",
		   now_us() - start_us, (res > 0) ? "still " : "not ");

	close(kq);

	return 0;
}

#endif							/* HAVE_SYS_EVENT_H */
