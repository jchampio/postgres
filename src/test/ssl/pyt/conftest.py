# Copyright (c) 2025, PostgreSQL Global Development Group

import pytest

import pg
from pg.fixtures import *


@pytest.fixture
def cryptography():
    return pytest.importorskip("cryptography", "40.0")
