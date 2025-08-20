# Copyright (c) 2025, PostgreSQL Global Development Group

import pytest
import pg

# This suite opens up TCP ports and is hidden behind PG_TEST_EXTRA=ssl.
pytestmark = pg.require_test_extra("ssl")


@pytest.fixture(scope="session")
def server_cert():
    return generate_cert("example.org")


@pytest.fixture
def client_cert():
    pass


def test_something(libpq):
    libpq.lib.PQconnectdb("hey, this doesn't work")


def test_else():
    pass
