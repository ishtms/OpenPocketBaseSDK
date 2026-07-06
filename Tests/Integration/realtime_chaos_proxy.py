#!/usr/bin/env python3

import argparse
import http.client
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class ChaosState:
    def __init__(self, upstream_host: str, upstream_port: int) -> None:
        self.upstream_host = upstream_host
        self.upstream_port = upstream_port
        self.subscription_posted = threading.Event()
        self.lock = threading.Lock()
        self.first_stream_claimed = False

    def claim_first_stream(self) -> bool:
        with self.lock:
            if self.first_stream_claimed:
                return False
            self.first_stream_claimed = True
            return True


class ChaosProxyHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "OpenPocketBaseChaosProxy/1"

    def do_GET(self) -> None:
        self._forward()

    def do_POST(self) -> None:
        self._forward()

    def do_PATCH(self) -> None:
        self._forward()

    def do_DELETE(self) -> None:
        self._forward()

    def log_message(self, format_string: str, *args: object) -> None:
        return

    def _forward(self) -> None:
        state: ChaosState = self.server.chaos_state
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length else None
        headers = {
            key: value
            for key, value in self.headers.items()
            if key.lower() not in {"host", "connection", "content-length"}
        }
        connection = http.client.HTTPConnection(
            state.upstream_host,
            state.upstream_port,
            timeout=10,
        )
        try:
            connection.request(self.command, self.path, body=body, headers=headers)
            response = connection.getresponse()
            is_stream = self.command == "GET" and self.path.startswith("/api/realtime")
            self.send_response(response.status, response.reason)
            for key, value in response.getheaders():
                if key.lower() in {"connection", "content-length", "transfer-encoding"}:
                    continue
                self.send_header(key, value)
            self.send_header("Connection", "close" if is_stream else "keep-alive")

            if is_stream:
                self.end_headers()
                self._forward_fragmented_stream(response, state)
                self.close_connection = True
                return

            response_body = response.read()
            self.send_header("Content-Length", str(len(response_body)))
            self.end_headers()
            if response_body:
                self.wfile.write(response_body)
                self.wfile.flush()
            if self.command == "POST" and self.path.startswith("/api/realtime") and 200 <= response.status < 300:
                state.subscription_posted.set()
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            self.close_connection = True
        finally:
            connection.close()

    def _forward_fragmented_stream(
        self,
        response: http.client.HTTPResponse,
        state: ChaosState,
    ) -> None:
        drop_this_stream = state.claim_first_stream()
        received = bytearray()
        while True:
            byte = response.read(1)
            if not byte:
                return
            self.wfile.write(byte)
            self.wfile.flush()
            received.extend(byte)
            time.sleep(0.001)
            if drop_this_stream and b"PB_CONNECT" in received and (
                received.endswith(b"\n\n") or received.endswith(b"\r\n\r\n")
            ):
                state.subscription_posted.wait(timeout=5)
                return


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--listen-host", default="127.0.0.1")
    parser.add_argument("--listen-port", type=int, required=True)
    parser.add_argument("--upstream-host", default="127.0.0.1")
    parser.add_argument("--upstream-port", type=int, required=True)
    args = parser.parse_args()

    server = ThreadingHTTPServer(
        (args.listen_host, args.listen_port),
        ChaosProxyHandler,
    )
    server.daemon_threads = True
    server.chaos_state = ChaosState(args.upstream_host, args.upstream_port)
    print(
        f"Realtime chaos proxy listening at http://{args.listen_host}:{args.listen_port}",
        flush=True,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
