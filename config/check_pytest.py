# Copyright (c) 2025, PostgreSQL Global Development Group
#
# Verify that pytest-requirements.txt is satisfied. This would probably be
# easier with pip, but requiring pip on build machines is a non-starter for
# many.
#
# The design philosophy of this script is to bend over backwards to help people
# figure out what is missing. The target audience for error output is the
# buildfarm operator who just wants to get the tests running, not the test
# developer who presumably already knows how to solve these problems.

import sys
from typing import List  # TODO: Python 3.9 will remove the need for this


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: python {} REQUIREMENTS_FILE".format(sys.argv[0]))

    requirements_file = sys.argv[1]
    with open(requirements_file, "r") as f:
        requirements = f.readlines()

    found = packaging_check(requirements)
    if not found:
        sys.exit("See src/test/pytest/README for package installation help.")


def packaging_check(requirements: List[str]) -> bool:
    """
    The preferred dependency check, which unfortunately needs newer Python
    facilities. Returns True if all dependencies were found.
    """
    try:
        # First, attempt to find importlib.metadata. This is part of the
        # standard library from 3.8 onwards. Earlier Python versions have an
        # official backport called importlib_metadata, which can generally be
        # installed as a separate OS package (e.g. python3-importlib-metadata).
        # This complication can be removed once we stop supporting Python 3.7.
        try:
            from importlib import metadata
        except ImportError:
            import importlib_metadata as metadata

        # packaging contains the PyPA definitions of requirement specifiers.
        # This is again contained in a separate OS package (for example,
        # python3-packaging).
        import packaging
        from packaging.requirements import Requirement

    except ImportError as err:
        # We don't even have enough prerequisites to check our prerequisites.
        # Try to fall back on the deprecated parser, to get a better error
        # message.
        found = setuptools_fallback(requirements)

        if not found:
            # Well, the best we can do is just print the import error as-is.
            print(err, file=sys.stderr)

        return False

    # Strip extraneous whitespace, whole-line comments, and empty lines from our
    # specifier list.
    requirements = [r.strip() for r in requirements]
    requirements = [r for r in requirements if r and r[0] != "#"]

    found = True
    for spec in requirements:
        req = Requirement(spec)

        # Skip any packages marked as unneeded for this particular Python env.
        if req.marker and not req.marker.evaluate():
            continue

        # Make sure the package is installed...
        try:
            version = metadata.version(req.name)
        except metadata.PackageNotFoundError:
            print("Package '{}' is not installed".format(req.name), file=sys.stderr)
            found = False
            continue

        # ...and that it has a compatible version.
        if not req.specifier.contains(version):
            print(
                "Package '{}' has version {}, but '{}' is required".format(
                    req.name, version, req.specifier
                ),
                file=sys.stderr,
            )
            found = False
            continue

    return found


def setuptools_fallback(requirements: List[str]) -> bool:
    """
    An alternative dependency helper, based on the old deprecated pkg_resources
    module in setuptools, which is pretty widely available in older Pythons. The
    point of this is to bootstrap the user into an environment that can run the
    packaging_check().

    Returns False if pkg_resources is also unavailable, in which case we just
    have to do our best.
    """
    try:
        import pkg_resources
    except ModuleNotFoundError:
        return False

    # An extra newline makes the Autoconf output easier to read.
    print(file=sys.stderr)

    # Go one-by-one through the requirements, printing each missing dependency.
    found = True
    for r in requirements:
        try:
            pkg_resources.require(r)
        except pkg_resources.DistributionNotFound as err:
            # The error descriptions given here are pretty good as-is.
            print(err, file=sys.stderr)
            found = False
        except pkg_resources.RequirementParseError as err:
            assert False  # TODO

    # The only reason the fallback would be called is if we're missing required
    # packages. So if we "found them", the requirements file is broken...
    assert (
        not found
    ), "setuptools_fallback() succeeded unexpectedly; is the requirements file incomplete?"

    return True


if __name__ == "__main__":
    main()
