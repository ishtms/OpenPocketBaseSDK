import http.server
import subprocess
import sys
import threading
import time


class Fixture(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    redirected_requests = 0
    target_port = 0

    def log_message(self, *_):
        pass

    def respond(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        if self.path == "/capture":
            type(self).redirected_requests += 1
        if self.path.startswith("/redirect/"):
            self.send_response(int(self.path.rsplit("/", 1)[1]))
            self.send_header("Location", f"http://127.0.0.1:{self.target_port}/capture")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/sse":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"event: PB_CONNECT\ndata: {}\n\n")
            self.wfile.flush()
            time.sleep(1)
            return
        if self.path == "/stall":
            self.send_response(200)
            self.send_header("Content-Length", "100")
            self.end_headers()
            time.sleep(1)
            return
        content = body if self.path == "/echo" else b"x" * (1024 * 1024) if self.path == "/large" else b"record"
        self.send_response(200)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    do_GET = do_POST = do_PATCH = do_DELETE = respond


def main():
    source = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    target = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    Fixture.target_port = target.server_port
    for server in (source, target):
        threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        result = subprocess.run([sys.argv[1], f"http://127.0.0.1:{source.server_port}"], timeout=30)
        if Fixture.redirected_requests:
            raise AssertionError("The redirect target received a request.")
        return result.returncode
    finally:
        for server in (source, target):
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    sys.exit(main())
