import os
import sys
from typing import Optional

import pytest


class TAP:
    def __init__(self):
        self.count = 0
        self.expected = 0

        # self.print("TAP version 13")

    def expect(self, num: int):
        self.print("1..{}".format(num))

    def print(self, *args):
        print(*args, file=sys.__stdout__)

    def ok(self, name: str):
        self.count += 1
        self.print("ok", self.count, "-", name)

    def skip(self, name: str, reason: str):
        self.count += 1
        self.print("ok", self.count, "-", name, "# skip", reason)

    def fail(self, name: str, details: str):
        self.count += 1
        self.print("not ok", self.count, "-", name)

        for l in details.splitlines():
            self.print("#", l)


tap = TAP()


class TestNotes:
    skipped = False
    skip_reason = None

    failed = False
    details = ""


notes_key = pytest.StashKey[TestNotes]()


@pytest.hookimpl(tryfirst=True)
def pytest_configure(config):
    """Hijack the standard streams."""
    logdir = os.getenv("TESTLOGDIR")
    if not logdir:
        raise RuntimeError("pgtap requires the TESTLOGDIR envvar to be set")

    os.makedirs(logdir)
    logpath = os.path.join(logdir, "pytest.log")
    sys.stdout = sys.stderr = open(logpath, "a", buffering=1)


@pytest.hookimpl(trylast=True)
def pytest_sessionfinish(session, exitstatus):
    """Suppresses errors due to failed tests."""
    if exitstatus == pytest.ExitCode.TESTS_FAILED:
        session.exitstatus = pytest.ExitCode.OK


@pytest.hookimpl
def pytest_collectreport(report):
    if report.failed:
        print(report.longreprtext, file=sys.__stderr__)


@pytest.hookimpl
def pytest_internalerror(excrepr, excinfo):
    print(excrepr, file=sys.__stderr__)


@pytest.hookimpl(hookwrapper=True)
def pytest_collection(session):
    res = yield
    tap.expect(session.testscollected)
    return res


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    res = yield

    if notes_key not in item.stash:
        item.stash[notes_key] = TestNotes()
    notes = item.stash[notes_key]

    report = res.get_result()
    if report.passed:
        pass

    elif report.skipped:
        notes.skipped = True
        _, _, notes.skip_reason = report.longrepr

    elif report.failed:
        notes.failed = True

        if report.when in ("setup", "teardown"):
            notes.details += "--- Failure during {} ---\n".format(report.when)
        notes.details += report.longreprtext + "\n"

    else:
        raise RuntimeError("pytest_runtest_makereport received unknown test status")

    return res


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_protocol(item, nextitem):
    res = yield

    assert notes_key in item.stash, "pgtap didn't annotate a test item?"
    notes = item.stash[notes_key]

    if notes.failed:
        tap.fail(item.nodeid, notes.details)
    elif notes.skipped:
        tap.skip(item.nodeid, notes.skip_reason)
    else:
        tap.ok(item.nodeid)

    return res
