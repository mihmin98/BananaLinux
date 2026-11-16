#!/usr/bin/env python3
"""The fork's stand-in RomM 5.4.1, for what upstream's (ui/tests/fake-romm.py) doesn't answer: it stands in front of
upstream's, answers the fork's endpoints itself and hands every other request on, as it came, so pairing, platforms,
games, devices and saves are upstream's stand-in's own.

    romm-kiwami.py <port file> <upstream port> <firmware dir> <log file>

It listens on 127.0.0.1, on a free port it writes to <port file> once it is ready, and appends each request to <log
file> as "<METHOD> <path>?<query>"; a device pairing's request also as "init scopes: <its requested_scopes, by commas>".
It notes each token's scopes from the answers it hands back (a device pairing's /api/auth/device/token, a pairing
code's /api/client-tokens/exchange, /test/token's, which gets upstream's seven when it names none), and a request of
a token it never saw answers as one with no scope.

What it answers, from RomM 5.4.1's code (rommapp/romm, tag 5.4.1):
  GET /api/firmware[?platform_id=]            backend/endpoints/firmware.py get_platform_firmware(): a list of
  GET /api/firmware/<id>                      FirmwareSchema (endpoints/responses/firmware.py), and one; the files of
  GET /api/firmware/<id>/content/<name>       <firmware dir>/<platform fs_slug>/, ids from 1 in the order of their
                                              folders and names; the content 404 for a file gone. Each needs
                                              firmware.read (403 {"detail": "Forbidden"}), as RomM checks the scope
                                              before what the request names
  GET /api/roms/<id>                          endpoints/roms/__init__.py get_rom(): the game as upstream's stand-in
                                              lists it, found in its platforms' listings (the fields banana-sync reads
                                              of RomM's DetailedRomSchema are a listed game's); 404 when none has it
  GET /api/roms?...&search_term=<s>           upstream's stand-in's listing of the platform, only the games the search
                                              finds, then limit and offset: handler/database/roms_handler.py
                                              _filter_by_search_term(), <s> split at "|" into terms, any of which, a
                                              term's words (split at blanks) each in the game's name or file name, case
                                              aside (PostgreSQL's way; MariaDB's FULLTEXT takes a word of three letters
                                              or more as the start of one of theirs, which a game's own name holds too)
The tests' own hooks, under /kiwami/ (no token):
  POST /kiwami/fault {"path": <regex on "METHOD /path">, "kind": "truncate", "count"?: 1}: the next matching answers
       cut to half their body, the whole length announced, the connection closed
"""
import hashlib
import http.client
import http.server
import json
import os
import re
import sys
import threading
import urllib.parse
import zlib

PORT_FILE, UPSTREAM, FW, LOG = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
UPSTREAM_SCOPES = ["me.read", "roms.read", "platforms.read", "assets.read", "assets.write", "devices.read", "devices.write"]
lock = threading.Lock()
TOKENS = {}     # token -> its scopes
FAULTS = []


def log(line):
    with lock, open(LOG, "a") as f:
        f.write(line + "\n")


class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json", cut=False):
        if not isinstance(body, bytes):
            body = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        if cut:
            self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body[:len(body) // 2] if cut else body)
        if cut:
            self.wfile.flush()
            self.close_connection = True

    def do_GET(self):
        self.route("GET")

    def do_POST(self):
        self.route("POST")

    def do_PUT(self):
        self.route("PUT")

    def do_DELETE(self):
        self.route("DELETE")

    def do_HEAD(self):
        self.route("HEAD")

    def route(self, method):
        u = urllib.parse.urlsplit(self.path)
        p = urllib.parse.unquote(u.path)
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n) if n else b""
        log(f"{method} {u.path}?{u.query}")
        if p.startswith("/kiwami/"):
            return self.hook(p, raw)
        if p == "/api/firmware" or p.startswith("/api/firmware/"):
            return self.firmware(method, p, urllib.parse.parse_qs(u.query))
        if method == "GET" and re.fullmatch(r"/api/roms/\d+", p):
            return self.rom(int(p.split("/")[3]))
        if method == "GET" and p == "/api/roms" and "search_term" in urllib.parse.parse_qs(u.query, keep_blank_values=True):
            return self.search(urllib.parse.parse_qsl(u.query, keep_blank_values=True))
        if p == "/api/auth/device/init" and method == "POST":
            try:
                log("init scopes: " + ",".join(json.loads(raw).get("requested_scopes") or []))
            except ValueError:
                pass
        self.forward(method, p, raw)

    # ---- everything else: upstream's stand-in's, the tokens it hands out noted
    def forward(self, method, p, raw):
        c = http.client.HTTPConnection("127.0.0.1", UPSTREAM, timeout=60)
        headers = {k: v for k, v in self.headers.items() if k.lower() not in ("host", "connection", "content-length")}
        c.request(method, self.path, body=raw if raw else None, headers=headers)
        r = c.getresponse()
        body = r.read()
        if r.status == 200 and r.getheader("Content-Type", "").startswith("application/json"):
            try:
                j = json.loads(body)
            except ValueError:
                j = None
            if isinstance(j, dict):
                tok = j.get("access_token") or j.get("raw_token") or j.get("token")
                if tok:
                    scopes = j.get("scopes")
                    if scopes is None and p == "/test/token":
                        scopes = (json.loads(raw) if raw else {}).get("scopes") or UPSTREAM_SCOPES
                    with lock:
                        TOKENS[tok] = set(scopes or [])
        self.send_response(r.status)
        for k, v in r.getheaders():
            if k.lower() not in ("connection", "transfer-encoding", "content-length", "date", "server"):
                self.send_header(k, v)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if method != "HEAD":
            self.wfile.write(body)
        c.close()

    def scopes(self):
        a = self.headers.get("Authorization", "")
        with lock:
            return TOKENS.get(a[7:], set()) if a.startswith("Bearer ") else None

    # ---- upstream's stand-in asked, with this request's token: (status, its JSON or None)
    def ask(self, path):
        c = http.client.HTTPConnection("127.0.0.1", UPSTREAM, timeout=60)
        c.request("GET", path, headers={"Authorization": self.headers.get("Authorization", "")})
        r = c.getresponse()
        body = r.read()
        c.close()
        try:
            return r.status, json.loads(body)
        except ValueError:
            return r.status, None

    # ---- firmware: <firmware dir>/<fs_slug>/<file>, on the platform upstream's stand-in lists for that folder
    def platforms(self):
        status, j = self.ask("/api/platforms")
        return {p["fs_slug"]: p["id"] for p in j} if status == 200 else {}

    # ---- a game alone, and RomM's search: from upstream's stand-in's listings
    def listing(self, pid, params=()):
        q = urllib.parse.urlencode([(k, v) for k, v in params if k not in ("platform_ids", "platform_id", "limit", "offset", "search_term")] +
                                   [("platform_ids", pid), ("limit", 100000), ("offset", 0), ("with_files", "true")])
        return self.ask("/api/roms?" + q)

    def rom(self, rid):
        status, ps = self.ask("/api/platforms")
        if status != 200:
            return self.send(status, ps)
        for p in ps:
            status, page = self.listing(p["id"])
            for it in (page or {}).get("items", []) if status == 200 else []:
                if it["id"] == rid:
                    return self.send(200, it)
        return self.send(404, {"detail": f"Rom with ID {rid} not found"})

    def search(self, qsl):
        q = dict(qsl)
        terms = [t.strip() for t in q.get("search_term", "").split("|") if t.strip()]
        status, page = self.listing(q.get("platform_ids") or q.get("platform_id") or "0", qsl)
        if status != 200:
            return self.send(status, page)
        found = lambda it: any(all(any(w.lower() in (it.get(k) or "").lower() for k in ("name", "fs_name")) for w in t.split()) for t in terms)
        items = [it for it in page["items"] if not terms or found(it)]
        lim, off = int(q.get("limit", 50)), int(q.get("offset", 0))
        return self.send(200, {"items": items[off:off + lim], "total": len(items), "limit": lim, "offset": off})

    def files(self):
        ids = self.platforms()
        out, i = [], 0
        for fs in sorted(os.listdir(FW)) if os.path.isdir(FW) else []:
            for name in sorted(os.listdir(os.path.join(FW, fs))):
                i += 1
                full = os.path.join(FW, fs, name)
                if fs not in ids or not os.path.isfile(full):
                    continue
                data = open(full, "rb").read()
                stem, ext = os.path.splitext(name)
                out.append({"id": i, "platform_id": ids[fs], "file_name": name, "file_name_no_tags": re.sub(r"\s*[(\[].*", "", stem),
                            "file_name_no_ext": stem, "file_extension": ext.lstrip("."), "file_path": f"{fs}/bios",
                            "file_size_bytes": len(data), "full_path": f"/romm/library/{fs}/bios/{name}", "is_verified": False,
                            "crc_hash": f"{zlib.crc32(data):08x}", "md5_hash": hashlib.md5(data).hexdigest(),
                            "sha1_hash": hashlib.sha1(data).hexdigest(), "missing_from_fs": False,
                            "created_at": "2026-10-01T10:00:00Z", "updated_at": "2026-10-01T10:00:00Z", "_path": full})
        return out

    def firmware(self, method, p, qa):
        s = self.scopes()
        if s is None:
            return self.send(401, {"detail": "Not authenticated"})
        if "firmware.read" not in s:
            return self.send(403, {"detail": "Forbidden"})
        fws = self.files()
        public = lambda f: {k: v for k, v in f.items() if not k.startswith("_")}
        if p == "/api/firmware" and method == "GET":
            pid = qa.get("platform_id", [""])[-1]
            if pid and not pid.isdigit():
                return self.send(422, {"detail": [{"type": "int_parsing", "loc": ["query", "platform_id"], "msg": "Input should be a valid integer"}]})
            return self.send(200, [public(f) for f in fws if not pid or int(pid) == 0 or f["platform_id"] == int(pid)])
        parts = p.split("/")
        if len(parts) < 4 or not parts[3].isdigit():
            return self.send(422, {"detail": [{"type": "int_parsing", "loc": ["path", "id"], "msg": "Input should be a valid integer"}]})
        fid = int(parts[3])
        f = next((x for x in fws if x["id"] == fid), None)
        if len(parts) == 4 and method == "GET":
            return self.send(200, public(f)) if f else self.send(404, {"detail": f"Firmware with ID {fid} not found"})
        if len(parts) >= 6 and parts[4] == "content" and method in ("GET", "HEAD"):
            if not f:
                return self.send(404, {"detail": f"Firmware with ID {fid} not found"})
            if not os.path.isfile(f["_path"]):
                return self.send(404, {"detail": f"Firmware file '{f['file_name']}' is missing from filesystem"})
            data = open(f["_path"], "rb").read()
            return self.send(200, data, "application/octet-stream", cut=self.fault(method, p) == "truncate")
        return self.send(405, {"detail": "Method Not Allowed"})

    def fault(self, method, p):
        with lock:
            for f in FAULTS:
                if f["count"] > 0 and re.search(f["path"], f"{method} {p}"):
                    f["count"] -= 1
                    return f["kind"]
        return None

    def hook(self, p, raw):
        b = json.loads(raw) if raw else {}
        if p == "/kiwami/fault":
            with lock:
                FAULTS.append({"path": b["path"], "kind": b["kind"], "count": int(b.get("count", 1))})
            return self.send(200, {})
        return self.send(404, {"detail": "Not Found"})


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True


srv = Server(("127.0.0.1", 0), H)
with open(PORT_FILE + ".new", "w") as f:
    f.write(f"{srv.server_address[1]}\n")
os.rename(PORT_FILE + ".new", PORT_FILE)
srv.serve_forever()
