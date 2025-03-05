/*
 * test-oauth-curl.c
 *
 * A unit test driver for libpq-oauth. This snippet is #include'd at the end of
 * oauth-curl.c when BUILD_OAUTH_UNIT_TESTS is defined, which lets the tests
 * reference static functions and other internals.
 *
 * USE_ASSERT_CHECKING is required, to make it easy for tests to wrap
 * must-succeed code as part of test setup.
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 */

#ifdef USE_ASSERT_CHECKING

/*
 * TAP Helpers
 */

static int	num_tests = 0;

/*
 * Reports ok/not ok to the TAP stream on stdout.
 */
static void
ok(bool ok, const char *test)
{
	printf("%sok %d - %s\n", ok ? "" : "not ", ++num_tests, test);
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

/*
 * Test Suites
 */

static void
test_set_timer(void)
{
	struct async_ctx *actx = init_test_actx();
	pg_usec_time_t now;
	int			res;

	now = PQgetCurrentTimeUSec();

	/* A zero-duration timer should result in a near-immediate ready signal. */
	Assert(set_timer(actx, 0));
	res = PQsocketPoll(actx->mux, 1, 0, now + 1000 * 1000 * 1000);

	Assert(res != -1);
	ok(res > 0, "expired timer: PQsocketPoll is ready");

	/* Resetting the timer far in the future should unset the ready signal. */
	Assert(set_timer(actx, INT_MAX));
	res = PQsocketPoll(actx->mux, 1, 0, 0);

	Assert(res != -1);
	ok(res == 0, "far-future timer, zero timeout: PQsocketPoll is not ready");

	/* Setting another zero-duration timer should override the previous one. */
	Assert(set_timer(actx, 0));
	res = PQsocketPoll(actx->mux, 1, 0, now + 1000 * 1000 * 1000);

	Assert(res != -1);
	ok(res > 0, "re-expired timer: PQsocketPoll is ready");

	/* And disabling that timer should once again unset the ready signal. */
	Assert(set_timer(actx, -1));
	res = PQsocketPoll(actx->mux, 1, 0, 0);

	Assert(res != -1);
	ok(res == 0, "disabled timer, zero timeout: PQsocketPoll is not ready");

	free_test_actx(actx);
}

int
main(int argc, char *argv[])
{
	/*
	 * Set up line buffering for our output, to let stderr interleave in the
	 * log files.
	 */
	setvbuf(stdout, NULL, PG_IOLBF, 0);

	test_set_timer();

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
