#! /usr/bin/env python3

import base64
import http.server
import json
import os
import sys
import time
import urllib.parse
from collections import defaultdict


class OAuthHandler(http.server.BaseHTTPRequestHandler):
    JsonObject = dict[str, object]  # TypeAlias is not available until 3.10

    def _check_issuer(self):
        """
        Switches the behavior of the provider depending on the issuer URI.
        """
        self._alt_issuer = self.path.startswith("/alternate/")
        self._parameterized = self.path.startswith("/param/")

        if self._alt_issuer:
            self.path = self.path.removeprefix("/alternate")
        elif self._parameterized:
            self.path = self.path.removeprefix("/param")

    def do_GET(self):
        self._response_code = 200
        self._check_issuer()

        if self.path == "/.well-known/openid-configuration":
            resp = self.config()
        else:
            self.send_error(404, "Not Found")
            return

        self._send_json(resp)

    def _parse_params(self) -> dict[str, str]:
        """
        Parses apart the form-urlencoded request body and returns the resulting
        dict. For use by do_POST().
        """
        size = int(self.headers["Content-Length"])
        form = self.rfile.read(size)

        assert self.headers["Content-Type"] == "application/x-www-form-urlencoded"
        return urllib.parse.parse_qs(form.decode("utf-8"), strict_parsing=True)

    @property
    def client_id(self) -> str:
        """
        Returns the client_id sent in the POST body. self._parse_params() must
        have been called first.
        """
        return self._params["client_id"][0]

    def do_POST(self):
        self._response_code = 200
        self._check_issuer()

        self._params = self._parse_params()
        if self._parameterized:
            # Pull encoded test parameters out of the peer's client_id field.
            # This is expected to be Base64-encoded JSON.
            js = base64.b64decode(self.client_id)
            self._test_params = json.loads(js)

        if self.path == "/authorize":
            resp = self.authorization()
        elif self.path == "/token":
            resp = self.token()
        else:
            self.send_error(404)
            return

        self._send_json(resp)

    def _should_modify(self) -> bool:
        """
        Returns True if the client has requested a modification to this stage of
        the exchange.
        """
        if not hasattr(self, "_test_params"):
            return False

        stage = self._test_params.get("stage")

        return (
            stage == "all"
            or (stage == "device" and self.path == "/authorize")
            or (stage == "token" and self.path == "/token")
        )

    def _content_type(self) -> str:
        """
        Returns "application/json" unless the test has requested something
        different.
        """
        if self._should_modify() and "content_type" in self._test_params:
            return self._test_params["content_type"]

        return "application/json"

    def _interval(self) -> int:
        """
        Returns 0 unless the test has requested something different.
        """
        if self._should_modify() and "interval" in self._test_params:
            return self._test_params["interval"]

        return 0

    def _retry_code(self) -> str:
        """
        Returns "authorization_pending" unless the test has requested something
        different.
        """
        if self._should_modify() and "retry_code" in self._test_params:
            return self._test_params["retry_code"]

        return "authorization_pending"

    def _uri_spelling(self) -> str:
        """
        Returns "verification_uri" unless the test has requested something
        different.
        """
        if self._should_modify() and "uri_spelling" in self._test_params:
            return self._test_params["uri_spelling"]

        return "verification_uri"

    def _send_json(self, js: JsonObject) -> None:
        """
        Sends the provided JSON dict as an application/json response.
        self._response_code can be modified to send JSON error responses.
        """
        resp = json.dumps(js).encode("ascii")
        self.log_message("sending JSON response: %s", resp)

        self.send_response(self._response_code)
        self.send_header("Content-Type", self._content_type())
        self.send_header("Content-Length", str(len(resp)))
        self.end_headers()

        self.wfile.write(resp)

    def config(self) -> JsonObject:
        port = self.server.socket.getsockname()[1]

        issuer = f"http://localhost:{port}"
        if self._alt_issuer:
            issuer += "/alternate"
        elif self._parameterized:
            issuer += "/param"

        return {
            "issuer": issuer,
            "token_endpoint": issuer + "/token",
            "device_authorization_endpoint": issuer + "/authorize",
            "response_types_supported": ["token"],
            "subject_types_supported": ["public"],
            "id_token_signing_alg_values_supported": ["RS256"],
            "grant_types_supported": ["urn:ietf:params:oauth:grant-type:device_code"],
        }

    @property
    def _token_state(self):
        """
        A cached _TokenState object for the connected client (as determined by
        the request's client_id), or a new one if it doesn't already exist.

        This relies on the existence of a defaultdict attached to the server;
        see main() below.
        """
        return self.server.token_state[self.client_id]

    def _remove_token_state(self):
        """
        Removes any cached _TokenState for the current client_id. Call this
        after the token exchange ends to get rid of unnecessary state.
        """
        if self.client_id in self.server.token_state:
            del self.server.token_state[self.client_id]

    def authorization(self) -> JsonObject:
        uri = "https://example.com/"
        if self._alt_issuer:
            uri = "https://example.org/"

        resp = {
            "device_code": "postgres",
            "user_code": "postgresuser",
            self._uri_spelling(): uri,
            "expires-in": 5,
        }

        interval = self._interval()
        if interval is not None:
            resp["interval"] = interval
            self._token_state.min_delay = interval
        else:
            self._token_state.min_delay = 5  # default

        return resp

    def token(self) -> JsonObject:
        if self._should_modify() and "retries" in self._test_params:
            retries = self._test_params["retries"]

            # Check to make sure the token interval is being respected.
            now = time.monotonic()
            if self._token_state.last_try is not None:
                delay = now - self._token_state.last_try
                assert (
                    delay > self._token_state.min_delay
                ), f"client waited only {delay} seconds between token requests (expected {self._token_state.min_delay})"

            self._token_state.last_try = now

            # If we haven't reached the required number of retries yet, return a
            # "pending" response.
            if self._token_state.retries < retries:
                self._token_state.retries += 1

                self._response_code = 400
                return {"error": self._retry_code()}

        # Clean up any retry tracking state now that the exchange is ending.
        self._remove_token_state()

        token = "9243959234"
        if self._alt_issuer:
            token += "-alt"

        return {
            "access_token": token,
            "token_type": "bearer",
        }


def main():
    s = http.server.HTTPServer(("127.0.0.1", 0), OAuthHandler)

    # Attach a "cache" dictionary to the server to allow the OAuthHandlers to
    # track state across token requests. The use of defaultdict ensures that new
    # entries will be created automatically.
    class _TokenState:
        retries = 0
        min_delay = None
        last_try = None

    s.token_state = defaultdict(_TokenState)

    # Give the parent the port number to contact (this is also the signal that
    # we're ready to receive requests).
    port = s.socket.getsockname()[1]
    print(port)

    stdout = sys.stdout.fileno()
    sys.stdout.close()
    os.close(stdout)

    s.serve_forever()  # we expect our parent to send a termination signal


if __name__ == "__main__":
    main()
