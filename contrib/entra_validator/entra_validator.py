#! /usr/bin/env python3
#
# Copyright (c) 2024, PostgreSQL Global Development Group

import argparse
import os
import sys
import urllib.parse
import urllib.request

import jwt
from jwt import PyJWKClient

JWKS_URI = "https://login.microsoftonline.com/d1eec608-476a-4b93-9fa4-c2511bdd5ad2/discovery/v2.0/keys"
AUDIENCE = "32def7d1-46be-474f-b5d6-83a181270401"


def read_token(fd):
    """
    Reads the (single) bearer token from the --token-fd file descriptor.
    """
    token = None

    with os.fdopen(fd) as f:
        for line in f:
            if token is not None:
                raise RuntimeError("multiple tokens provided via --token-fd")

            token = line

    return token


def validate(token, *, issuer, required_scopes) -> dict:
    """
    Validates the token against the supplied --issuer, per the MS instructions
    at

        https://learn.microsoft.com/en-us/entra/identity-platform/access-tokens#validate-tokens

    The set of claims is returned.
    """
    print(jwt.get_unverified_header(token), file=sys.stderr)

    # First fetch the key document and pull the signing material we need for
    # this token.
    # TODO: don't hard-code the URI; get that from discovery instead.
    jwks = PyJWKClient(JWKS_URI)
    jwk = jwks.get_signing_key_from_jwt(token)

    # Double-check the key issuer, per MS instructions.
    # XXX this uses internal APIs...
    key_iss = jwk._jwk_data.get("issuer", None)
    if key_iss != issuer:
        raise RuntimeError(
            f"token's signing JWK (ID {jwk.key_id}) is issued by {key_iss}; expected {issuer}"
        )

    # Decoding the claims validates the signature.
    claims = jwt.decode(
        token,
        jwk.key,
        algorithms=["RS256"],
        audience=AUDIENCE,
        strict_aud=True,
        issuer=issuer,
        require=["exp", "iat", "tid", "scp"],
    )
    print(claims, file=sys.stderr)

    # MS instructions say to validate the tenant ID ("tid") against the issuer
    # as well.
    tid = claims["tid"]
    if not urllib.parse.urlparse(issuer).path.startswith(f"/{tid}/"):
        raise RuntimeError(f"token's tid claim does not match issuer {issuer}")

    # Split apart the scope claim and make sure our list is contained.
    scopes = claims["scp"].split()
    if not set(required_scopes).issubset(scopes):
        raise RuntimeError(f"token does not claim required scopes {required_scopes}")

    return claims


def main(argv):
    parser = argparse.ArgumentParser(prog="entra_validator", add_help=False)
    parser.add_argument("--token-fd", type=int, required=True)
    parser.add_argument("--issuer", type=str, required=True)
    parser.add_argument("--required-scopes", dest="scopes", type=str)

    args = parser.parse_args(argv[1:])

    token = read_token(args.token_fd)
    claims = validate(token, issuer=args.issuer, required_scopes=args.scopes.split())

    # Print out the identity of the user.
    authn_id = claims["oid"]
    if "preferred_username" in claims:
        authn_id += ":" + claims["preferred_username"]
    print(authn_id)


if __name__ == "__main__":
    main(sys.argv)
