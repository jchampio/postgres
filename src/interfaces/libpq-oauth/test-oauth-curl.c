/*
 * test-oauth-curl.c
 *
 * A unit test driver for libpq-oauth. This #includes oauth-curl.c, which lets
 * the tests reference static functions and other internals.
 *
 * USE_ASSERT_CHECKING is required, to make it easy for tests to wrap
 * must-succeed code as part of test setup.
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 */

#include "oauth-curl.c"

#include <fcntl.h>

#ifdef USE_ASSERT_CHECKING

/*
 * TAP Helpers
 */

static int	num_tests = 0;

/*
 * Reports ok/not ok to the TAP stream on stdout.
 */
#define ok(OK, TEST) \
	ok_impl(OK, TEST, #OK, __FILE__, __LINE__)

static bool
ok_impl(bool ok, const char *test, const char *teststr, const char *file, int line)
{
	printf("%sok %d - %s\n", ok ? "" : "not ", ++num_tests, test);

	if (!ok)
	{
		printf("# at %s:%d:\n", file, line);
		printf("#   expression is false: %s\n", teststr);
	}

	return ok;
}

/*
 * Like ok(this == that), but with more diagnostics on failure.
 *
 * Only works on ints, but luckily that's all we need here. Note that the much
 * simpler-looking macro implementation
 *
 *     is_diag(ok(THIS == THAT, TEST), THIS, #THIS, THAT, #THAT)
 *
 * suffers from multiple evaluation of the macro arguments...
 */
#define is(THIS, THAT, TEST) \
	do { \
		int this_ = (THIS), \
			that_ = (THAT); \
		is_diag( \
			ok_impl(this_ == that_, TEST, #THIS " == " #THAT, __FILE__, __LINE__), \
			this_, #THIS, that_, #THAT \
		); \
	} while (0)

static bool
is_diag(bool ok, int this, const char *thisstr, int that, const char *thatstr)
{
	if (!ok)
		printf("#   %s = %d; %s = %d\n", thisstr, this, thatstr, that);

	return ok;
}

/*
 * Utilities
 */

/*
 * Creates a partially-initialized async_ctx for the purposes of testing. Free
 * with free_test_actx().
 */
static struct async_ctx *
init_test_actx(void)
{
	struct async_ctx *actx;

	actx = calloc(1, sizeof(*actx));
	Assert(actx);

	actx->mux = PGINVALID_SOCKET;
	actx->timerfd = -1;
	actx->debugging = true;

	initPQExpBuffer(&actx->errbuf);

	Assert(setup_multiplexer(actx));

	return actx;
}

static void
free_test_actx(struct async_ctx *actx)
{
	termPQExpBuffer(&actx->errbuf);

	if (actx->mux != PGINVALID_SOCKET)
		close(actx->mux);
	if (actx->timerfd >= 0)
		close(actx->timerfd);

	free(actx);
}

static char dummy_buf[4 * 1024];	/* for fill_pipe/drain_pipe */

/*
 * Writes to the write side of a pipe until it won't take any more data. Returns
 * the amount written.
 */
static ssize_t
fill_pipe(int fd)
{
	int			mode;
	ssize_t		written = 0;

	/* Don't block. */
	Assert((mode = fcntl(fd, F_GETFL)) != -1);
	Assert(fcntl(fd, F_SETFL, mode | O_NONBLOCK) == 0);

	while (true)
	{
		ssize_t		w;

		w = write(fd, dummy_buf, sizeof(dummy_buf));
		if (w < 0)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				perror("write to pipe");
				written = -1;
			}
			break;
		}

		written += w;
	}

	/* Reset the descriptor flags. */
	Assert(fcntl(fd, F_SETFD, mode) == 0);

	return written;
}

/*
 * Drains the requested amount of data from the read side of a pipe.
 */
static bool
drain_pipe(int fd, ssize_t n)
{
	Assert(n > 0);

	while (n)
	{
		size_t		to_read = (n <= sizeof(dummy_buf)) ? n : sizeof(dummy_buf);
		ssize_t		drained;

		drained = read(fd, dummy_buf, to_read);
		if (drained < 0)
		{
			perror("read from pipe");
			return false;
		}

		n -= drained;
	}

	return true;
}

/*
 * Tests whether the multiplexer is marked ready by the deadline. This is a
 * macro so that file/line information makes sense during failures.
 *
 * NB: our current multiplexer implementations (epoll/kqueue) are *readable*
 * when the underlying libcurl sockets are *writable*. This behavior is pinned
 * here to record that expectation, but it's not a required part of the API. If
 * you've added a new implementation that doesn't have that behavior, feel free
 * to modify this test.
 */
#define mux_is_ready(MUX, DEADLINE, TEST) \
	do { \
		int res_ = PQsocketPoll(MUX, 1, 0, DEADLINE); \
		Assert(res_ != -1); \
		ok(res_ > 0, "multiplexer is ready " TEST); \
	} while (0)

/*
 * The opposite of mux_is_ready().
 */
#define mux_is_not_ready(MUX, TEST) \
	do { \
		int res_ = PQsocketPoll(MUX, 1, 0, 0); \
		Assert(res_ != -1); \
		is(res_, 0, "multiplexer is not ready " TEST); \
	} while (0)

/*
 * Test Suites
 */

/* Per-suite timeout. Set via the PG_TEST_TIMEOUT_DEFAULT envvar. */
static pg_usec_time_t timeout_us = 180 * 1000 * 1000;

static void
test_set_timer(void)
{
	struct async_ctx *actx = init_test_actx();
	const pg_usec_time_t deadline = PQgetCurrentTimeUSec() + timeout_us;

	printf("# test_set_timer\n");

	/* A zero-duration timer should result in a near-immediate ready signal. */
	Assert(set_timer(actx, 0));
	mux_is_ready(actx->mux, deadline, "when timer expires");
	is(timer_expired(actx), 1, "timer_expired() returns 1 when timer expires");

	/* Resetting the timer far in the future should unset the ready signal. */
	Assert(set_timer(actx, INT_MAX));
	mux_is_not_ready(actx->mux, "when timer is reset to the future");
	is(timer_expired(actx), 0, "timer_expired() returns 0 with unexpired timer");

	/* Setting another zero-duration timer should override the previous one. */
	Assert(set_timer(actx, 0));
	mux_is_ready(actx->mux, deadline, "when timer is re-expired");
	is(timer_expired(actx), 1, "timer_expired() returns 1 when timer is re-expired");

	/* And disabling that timer should once again unset the ready signal. */
	Assert(set_timer(actx, -1));
	mux_is_not_ready(actx->mux, "when timer is unset");
	is(timer_expired(actx), 0, "timer_expired() returns 0 when timer is unset");

	{
		bool		expired;

		/* Make sure drain_timer_events() functions correctly as well. */
		Assert(set_timer(actx, 0));
		mux_is_ready(actx->mux, deadline, "when timer is re-expired (drain_timer_events)");

		Assert(drain_timer_events(actx, &expired));
		mux_is_not_ready(actx->mux, "when timer is drained after expiring");
		is(expired, 1, "drain_timer_events() reports expiration");
		is(timer_expired(actx), 0, "timer_expired() returns 0 after timer is drained");

		/* A second drain should do nothing. */
		Assert(drain_timer_events(actx, &expired));
		mux_is_not_ready(actx->mux, "when timer is drained a second time");
		is(expired, 0, "drain_timer_events() reports no expiration");
		is(timer_expired(actx), 0, "timer_expired() still returns 0");
	}

	free_test_actx(actx);
}

static void
test_register_socket(void)
{
	struct async_ctx *actx = init_test_actx();
	int			pipefd[2];
	int			rfd,
				wfd;

	/* Create a local pipe for communication. */
	Assert(pipe(pipefd) == 0);
	rfd = pipefd[0];
	wfd = pipefd[1];

	/*
	 * This suite runs twice -- once using CURL_POLL_IN/CURL_POLL_OUT for
	 * read/write operations, respectively, and once using CURL_POLL_INOUT for
	 * both sides.
	 */
	for (int inout = 0; inout < 2; inout++)
	{
		const int	in_event = inout ? CURL_POLL_INOUT : CURL_POLL_IN;
		const int	out_event = inout ? CURL_POLL_INOUT : CURL_POLL_OUT;
		const pg_usec_time_t deadline = PQgetCurrentTimeUSec() + timeout_us;

		printf("# test_register_socket %s\n", inout ? "(INOUT)" : "");

		/* Listen on the read side. The multiplexer shouldn't be ready yet. */
		Assert(register_socket(NULL, rfd, in_event, actx, NULL) == 0);
		mux_is_not_ready(actx->mux, "when fd is not readable");

		/* Writing to the pipe should result in a read-ready multiplexer. */
		Assert(write(wfd, "x", 1) == 1);
		mux_is_ready(actx->mux, deadline, "when fd is readable");

		/*
		 * Update the registration to wait on write events instead. The
		 * multiplexer should be unset.
		 */
		Assert(register_socket(NULL, rfd, CURL_POLL_OUT, actx, NULL) == 0);
		mux_is_not_ready(actx->mux, "when waiting for writes on readable fd");

		/* Re-register for read events. */
		Assert(register_socket(NULL, rfd, in_event, actx, NULL) == 0);
		mux_is_ready(actx->mux, deadline, "when waiting for reads again");

		/* Stop listening. The multiplexer should be unset. */
		Assert(register_socket(NULL, rfd, CURL_POLL_REMOVE, actx, NULL) == 0);
		mux_is_not_ready(actx->mux, "when readable fd is removed");

		/* Listen again. */
		Assert(register_socket(NULL, rfd, in_event, actx, NULL) == 0);
		mux_is_ready(actx->mux, deadline, "when readable fd is re-added");

		/*
		 * Draining the pipe should unset the multiplexer again, once the old
		 * event is drained.
		 */
		Assert(drain_pipe(rfd, 1));
		Assert(drain_socket_events(actx));
		mux_is_not_ready(actx->mux, "when fd is drained");

		/* Listen on the write side. An empty buffer should be writable. */
		Assert(register_socket(NULL, rfd, CURL_POLL_REMOVE, actx, NULL) == 0);
		Assert(register_socket(NULL, wfd, out_event, actx, NULL) == 0);
		mux_is_ready(actx->mux, deadline, "when fd is writable");

		/* As above, wait on read events instead. */
		Assert(register_socket(NULL, wfd, CURL_POLL_IN, actx, NULL) == 0);
		mux_is_not_ready(actx->mux, "when waiting for reads on writable fd");

		/* Re-register for write events. */
		Assert(register_socket(NULL, wfd, out_event, actx, NULL) == 0);
		mux_is_ready(actx->mux, deadline, "when waiting for writes again");

		{
			ssize_t		written;

			/*
			 * Fill the pipe. Once the old writable event is drained, the mux
			 * should not be ready.
			 */
			Assert((written = fill_pipe(wfd)) > 0);
			printf("# pipe buffer is full at %zd bytes\n", written);

			Assert(drain_socket_events(actx));
			mux_is_not_ready(actx->mux, "when fd buffer is full");

			/* Drain the pipe again. */
			Assert(drain_pipe(rfd, written));
			mux_is_ready(actx->mux, deadline, "when fd buffer is drained");
		}

		/* Stop listening. */
		Assert(register_socket(NULL, wfd, CURL_POLL_REMOVE, actx, NULL) == 0);
		mux_is_not_ready(actx->mux, "when fd is removed");
	}

	close(rfd);
	close(wfd);
	free_test_actx(actx);
}

int
main(int argc, char *argv[])
{
	const char *timeout;

	/* Grab the default timeout. */
	timeout = getenv("PG_TEST_TIMEOUT_DEFAULT");
	if (timeout)
	{
		int			timeout_s = atoi(timeout);

		if (timeout_s > 0)
			timeout_us = timeout_s * 1000 * 1000;
	}

	/*
	 * Set up line buffering for our output, to let stderr interleave in the
	 * log files.
	 */
	setvbuf(stdout, NULL, PG_IOLBF, 0);

	test_set_timer();
	test_register_socket();

	printf("1..%d\n", num_tests);
	return 0;
}

#else							/* !USE_ASSERT_CHECKING */

/*
 * Skip the test suite when we don't have assertions.
 */
int
main(int argc, char *argv[])
{
	printf("1..0 # skip: cassert is not enabled\n");

	return 0;
}

#endif							/* USE_ASSERT_CHECKING */
