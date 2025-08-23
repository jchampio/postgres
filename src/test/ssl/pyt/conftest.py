# Copyright (c) 2025, PostgreSQL Global Development Group

import datetime
import os
import socket
import subprocess
import tempfile
from collections import namedtuple

import pytest

import pg
from pg.fixtures import *


@pytest.fixture(scope="session")
def cryptography():
    return pytest.importorskip("cryptography", "40.0")


Cert = namedtuple("Cert", "cert, certpath, key, keypath")


@pytest.fixture(scope="session")
def certs(cryptography, tmp_path_factory):
    """
    Caches commonly used certificates at the session level, and provides a way
    to create new ones.

    - certs.ca: the root CA certificate

    - certs.server: the "standard" server certficate, signed by certs.ca

    - certs.server_host: the hostname of the certs.server certificate

    - certs.new(): creates a custom certificate, signed by certs.ca
    """

    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID

    tmpdir = tmp_path_factory.mktemp("test-certs")

    class _Certs:
        def __init__(self):
            self.ca = self.new(
                x509.Name(
                    [x509.NameAttribute(NameOID.COMMON_NAME, "PG pytest CA")],
                ),
                ca=True,
            )

            self.server_host = "example.org"
            self.server = self.new(
                x509.Name(
                    [x509.NameAttribute(NameOID.COMMON_NAME, self.server_host)],
                )
            )

        def new(self, subject: x509.Name, *, ca=False) -> Cert:
            """
            Creates and signs a new Cert with the given subject name. If ca is
            True, the certificate will be self-signed; otherwise the certificate
            is signed by self.ca.
            """
            key = rsa.generate_private_key(
                public_exponent=65537,
                key_size=2048,
            )

            builder = x509.CertificateBuilder()
            now = datetime.datetime.utcnow()

            builder = (
                builder.subject_name(subject)
                .public_key(key.public_key())
                .serial_number(x509.random_serial_number())
                .not_valid_before(now)
                .not_valid_after(now + datetime.timedelta(hours=1))
            )

            if ca:
                builder = builder.issuer_name(subject)
            else:
                builder = builder.issuer_name(self.ca.cert.subject)

            builder = builder.add_extension(
                x509.BasicConstraints(ca=ca, path_length=None),
                critical=True,
            )

            cert = builder.sign(
                private_key=key if ca else self.ca.key,
                algorithm=hashes.SHA256(),
            )

            # Dump the certificate and key to file.
            keypath = self._tofile(
                key.private_bytes(
                    serialization.Encoding.PEM,
                    serialization.PrivateFormat.PKCS8,
                    serialization.NoEncryption(),
                ),
                suffix=".key",
            )
            certpath = self._tofile(
                cert.public_bytes(serialization.Encoding.PEM),
                suffix="-ca.crt" if ca else ".crt",
            )

            return Cert(
                cert=cert,
                certpath=certpath,
                key=key,
                keypath=keypath,
            )

        def _tofile(self, data: bytes, *, suffix) -> str:
            """
            Dumps data to a file on disk with the requested suffix and returns
            the path. The file is located somewhere in pytest's temporary
            directory root.
            """
            f = tempfile.NamedTemporaryFile(suffix=suffix, dir=tmpdir, delete=False)
            with f:
                f.write(data)

            return f.name

    return _Certs()


@pytest.fixture(scope="session")
def server_instance(certs, tmp_path_factory):
    datadir = os.getenv("TESTDATADIR")
    if not datadir:
        datadir = tmp_path_factory.mktemp("tmp_check")

    subprocess.check_call(["pg_ctl", "-D", datadir, "init"])

    # Figure out a port to listen on. Attempt to reserve both IPv4 and IPv6
    # addresses in one go.
    #
    # Note: socket.has_dualstack_ipv6/create_server are only in Python 3.8+.
    if hasattr(socket, "has_dualstack_ipv6") and socket.has_dualstack_ipv6():
        addr = ("::1", 0)
        s = socket.create_server(addr, family=socket.AF_INET6, dualstack_ipv6=True)

        hostaddr, port, _, _ = s.getsockname()
        addrs = [hostaddr, "127.0.0.1"]

    else:
        addr = ("127.0.0.1", 0)

        s = socket.socket()
        s.bind(addr)

        hostaddr, port = s.getsockname()
        addrs = [hostaddr]

    with s, open(os.path.join(datadir, "postgresql.conf"), "a") as f:
        print(file=f)
        print("unix_socket_directories = ''", file=f)
        print("listen_addresses = '{}'".format(",".join(addrs)), file=f)
        print("port =", port, file=f)

    subprocess.check_call(["pg_ctl", "-D", datadir, "start"])
    yield (hostaddr, port)
    subprocess.check_call(["pg_ctl", "-D", datadir, "stop"])
