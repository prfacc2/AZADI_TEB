#!/usr/bin/env python3
"""Mock loopback host that mimics the C++ /api bridge so we can verify the
HTML/CSS/JS print designer (save round-trip, templates, no console errors)
exactly as the embedded host would serve it."""
import json, os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets", "designer")
SAVED = {}  # id -> design

CT = {
    "/": "text/html; charset=utf-8", "/index.html": "text/html; charset=utf-8",
    "/designer.css": "text/css", "/bootstrap.min.css": "text/css",
    "/fields.js": "application/javascript", "/templates.js": "application/javascript",
    "/designer.js": "application/javascript",
}
FILE = {
    "/": "index.html", "/index.html": "index.html", "/designer.css": "designer.css",
    "/bootstrap.min.css": "bootstrap.min.css", "/fields.js": "fields.js",
    "/templates.js": "templates.js", "/designer.js": "designer.js",
}

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, ctype, body):
        if isinstance(body, str): body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        p = self.path.split("?")[0]
        if p in FILE:
            with open(os.path.join(ASSETS, FILE[p]), "rb") as f:
                self._send(200, CT[p], f.read())
        else:
            self._send(404, "text/plain", "nf")
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(n).decode("utf-8") if n else "{}"
        verb = self.path[5:].split("?")[0] if self.path.startswith("/api/") else ""
        try: args = json.loads(body) if body else {}
        except: args = {}
        out = {"ok": False}
        if verb == "init":
            out = {"design": None, "sectionName": "پذیرش تست"}
        elif verb == "templates":
            out = {"templates": []}  # JS gallery used
        elif verb == "save":
            d = args.get("design") or {}
            did = d.get("id") or (max(list(SAVED.keys()) + [0]) + 1)
            d["id"] = did; SAVED[did] = d
            out = {"ok": True, "id": did}
        elif verb in ("ready", "exit", "download"):
            out = {"ok": True}
        self._send(200, "application/json; charset=utf-8", json.dumps(out, ensure_ascii=False))

if __name__ == "__main__":
    srv = ThreadingHTTPServer(("127.0.0.1", 8799), H)
    print("mock host on http://127.0.0.1:8799/")
    srv.serve_forever()
