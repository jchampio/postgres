# Copyright (c) 2025, PostgreSQL Global Development Group

import ctypes
import platform

import pytest


@pytest.fixture(scope="session")
def libpq():
    """
    Loads a ctypes handle to libpq.
    """
    system = platform.system()

    if system in ("Linux", "FreeBSD", "NetBSD", "OpenBSD"):
        name = "libpq.so.5"
    elif system == "Darwin":
        name = "libpq.5.dylib"
    elif system == "Windows":
        name = "libpq.dll"
    else:
        assert False, "the libpq fixture must be updated for {}".format(system)

    return ctypes.CDLL(name)
