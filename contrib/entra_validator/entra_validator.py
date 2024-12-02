#! /usr/bin/env python3
#
# Copyright (c) 2024, PostgreSQL Global Development Group

import argparse
import os
import sys
import urllib.request

import jwt
from jwt import PyJWKClient


JWKS_URI = "https://login.microsoftonline.com/d1eec608-476a-4b93-9fa4-c2511bdd5ad2/discovery/v2.0/keys"
AUDIENCE = "32def7d1-46be-474f-b5d6-83a181270401"


def read_token(fd):
    token = None

    with os.fdopen(fd) as f:
        for line in f:
            if token is not None:
                raise RuntimeError("multiple tokens provided via --token-fd")

            token = line

    return token


def validate(token):
    print(jwt.get_unverified_header(token), file=sys.stderr)

    jwks = PyJWKClient(JWKS_URI)
    pubkey = jwks.get_signing_key_from_jwt(token).key

    decoded = jwt.decode(token, pubkey, audience=AUDIENCE, algorithms=["RS256"])
    print(decoded, file=sys.stderr)

    sys.exit("validation not implemented")


def main(argv):
    parser = argparse.ArgumentParser(prog="entra_validator", add_help=False)
    parser.add_argument("--token-fd", type=int, required=True)

    args = parser.parse_args(argv[1:])

    token = read_token(args.token_fd)
    validate(token)


if __name__ == "__main__":
    main(sys.argv)
