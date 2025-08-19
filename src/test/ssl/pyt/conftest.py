# Copyright (c) 2025, PostgreSQL Global Development Group

import pytest

import pg


@pytest.fixture
def cryptography():
    return pytest.importorskip("cryptography", "40.0")
