#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import tempfile
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(ROOT, "build", "cparser")
EXAMPLES = os.path.join(ROOT, "examples")
MAX_STEPS = "2000000"
TIMEOUT_SECONDS = 10


def list_examples():
    result = []
    for folder, _, files in sorted(os.walk(EXAMPLES)):
        for name in sorted(files):
            if name.endswith(".cpp"):
                path = os.path.join(folder, name)
                with open(path, encoding="utf-8") as source:
                    result.append({"name": os.path.relpath(path, EXAMPLES), "code": source.read()})
    return result


def failure(message):
    return {
        "ok": False,
        "stage": "server",
        "diagnostics": [{"stage": "server", "line": 0, "col": 0, "message": message}],
        "ast": None,
        "js": "",
        "ran": False,
        "output": "",
        "exitCode": -1,
        "runtimeError": None,
    }


def parse(code):
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False, encoding="utf-8") as source:
        source.write(code)
        path = source.name
    try:
        completed = subprocess.run(
            [BINARY, path, "--json", "-", "--run", "--max-steps", MAX_STEPS],
            capture_output=True,
            text=True,
            timeout=TIMEOUT_SECONDS,
        )
        return json.loads(completed.stdout)
    except subprocess.TimeoutExpired:
        return failure(f"timeout: the parser ran longer than {TIMEOUT_SECONDS} s")
    except json.JSONDecodeError:
        return failure("cannot read parser output: " + completed.stderr.strip())
    finally:
        os.unlink(path)


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self.send_response(302)
            self.send_header("Location", "/web/index.html")
            self.end_headers()
            return
        if self.path == "/api/examples":
            self.send_json(list_examples())
            return
        super().do_GET()

    def do_POST(self):
        if self.path != "/api/parse":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length) or b"{}")
        self.send_json(parse(payload.get("code", "")))

    def send_json(self, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        sys.stderr.write("[server] " + format % args + "\n")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    if not os.path.exists(BINARY):
        print("build/cparser not found - run 'make' first")
        sys.exit(1)
    server = ThreadingHTTPServer(("127.0.0.1", port), partial(Handler, directory=ROOT))
    print(f"Open http://127.0.0.1:{port}/ in the browser (Ctrl+C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
