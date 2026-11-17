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
  GET /api/roms?platform_ids=<p>&...          upstream's stand-in's listing of the platform, each game with RomM's
                                              updated_at (its file's time, or the time a test touched it) and
                                              name_sort_key (models/rom.py compute_name_sort_key()), in RomM's name
                                              order (name_sort_key, then id: roms_handler.py
                                              _resolve_gallery_sort_key(), _gallery_order_clauses()), only the games
                                              updated_after and search_term keep, then limit and offset:
                                              updated_after, a game updated after it (roms_handler.py filter_roms());
                                              search_term, handler/database/roms_handler.py
                                              _filter_by_search_term(), <s> split at "|" into terms, any of which, a
                                              term's words (split at blanks) each in the game's name or file name, case
                                              aside (PostgreSQL's way; MariaDB's FULLTEXT takes a word of three letters
                                              or more as the start of one of theirs, which a game's own name holds too)
  GET /api/roms/identifiers                   endpoints/roms/__init__.py get_rom_identifiers(): every game's id, of
                                              upstream's stand-in's platforms' listings
  GET  /api/devices/<id>/installs             endpoints/device/install.py and handler/device_install/: a device's
  POST /api/devices/<id>/installs {rom_id}    install requests, pending or taken, oldest first; one queued (as the
  POST /api/devices/<id>/installs/claim       web page does: 400 unless the device's capabilities, upstream's
  PUT  /api/devices/<id>/installs/<request>   stand-in's, hold remote_install: true, file_ids its files as
                                              policy.py select_install_files() picks them, 200 with the live one for
                                              the same game); the pending taken and every taken one returned; one
                                              reported done, already_installed or failed (reason of 500 characters at
                                              most, else 422) and so ended, 404 when gone, 409 when not taken. Claim and
                                              report are the device's own (device_only): a token RomM's device pairing
                                              made for that device (its /api/auth/device/token answer), else 403. A
                                              device upstream's stand-in doesn't have is a 404
Each answer has a Date header, the server's time (Python's http.server, as uvicorn sends it). A device's PUT is
handed on, its capabilities also logged as "capabilities <device>: <JSON>".
The tests' own hooks, under /kiwami/ (no token):
  POST /kiwami/fault {"path": <regex on "METHOD /path">, "kind": "truncate"|"500", "count"?: 1}: the next matching
       answers cut to half their body, the whole length announced, the connection closed; or refused with a 500
  POST /kiwami/touch {"id", "name"?}: the game updated now (its updated_at), renamed when a name is given (a title
       edited in RomM's web UI)
  POST /kiwami/expire {"id"?}: that install request (or every one) gone, as RomM's two days running out do
  GET  /kiwami/installs: {"live": [...], "ended": [...]}, the install requests live and those ended, in order
"""
import datetime
import hashlib
import http.client
import http.server
import json
import os
import re
import sys
import threading
import time
import urllib.parse
import zlib

PORT_FILE, UPSTREAM, FW, LOG = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
UPSTREAM_SCOPES = ["me.read", "roms.read", "platforms.read", "assets.read", "assets.write", "devices.read", "devices.write"]
lock = threading.Lock()
TOKENS = {}     # token -> its scopes
FAULTS = []
TOUCHED = {}    # a game's id -> {"at": its updated_at (unix time), "name"?: its new name}
BOUND = {}      # token -> the device RomM's device pairing made it for
INSTALLS = []   # the live install requests, oldest first
ENDED = []      # those reported (or expired), in order
DEVICE_REPORTS = ("done", "already_installed", "failed")
# models/rom.py's ARTICLES and compute_name_sort_key()
ARTICLES = ("the", "a", "an", "le", "la", "les", "el", "los", "las", "il", "lo", "gli", "der", "die", "das", "het")


def name_sort_key(name):
    v = re.sub(rf"^({'|'.join(ARTICLES)})\s+", "", (name or "").lower()).strip()
    return re.sub(r"\d+", lambda m: m.group(0).zfill(12), v)[:500]


def utc(ts):
    return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).isoformat()


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
        if method == "GET" and p == "/api/roms/identifiers":
            return self.identifiers()
        qa = urllib.parse.parse_qs(u.query, keep_blank_values=True)
        if method == "GET" and p == "/api/roms" and ("platform_ids" in qa or "platform_id" in qa):
            return self.roms(urllib.parse.parse_qsl(u.query, keep_blank_values=True))
        if p == "/api/auth/device/init" and method == "POST":
            try:
                log("init scopes: " + ",".join(json.loads(raw).get("requested_scopes") or []))
            except ValueError:
                pass
        m = re.fullmatch(r"/api/devices/([^/]+)/installs(?:/([^/]+))?", p)
        if m:
            return self.installs(method, m.group(1), m.group(2), raw)
        if method == "PUT" and re.fullmatch(r"/api/devices/[^/]+", p):
            try:
                b = json.loads(raw)
                if isinstance(b, dict) and "capabilities" in b:
                    log(f"capabilities {p.split('/')[3]}: {json.dumps(b['capabilities'], sort_keys=True)}")
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
                        BOUND[tok] = j.get("device_id") if "access_token" in j else None
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

    # ---- a game alone, a platform's games and RomM's search: from upstream's stand-in's listings, each game with
    # what RomM 5.4.1 gives beside what upstream's does (updated_at, name_sort_key), in RomM's name order
    def listing(self, pid, params=()):
        q = urllib.parse.urlencode([(k, v) for k, v in params if k not in ("platform_ids", "platform_id", "limit", "offset", "search_term", "updated_after")] +
                                   [("platform_ids", pid), ("limit", 100000), ("offset", 0), ("with_files", "true")])
        status, page = self.ask("/api/roms?" + q)
        if status == 200 and isinstance(page, dict):
            for it in page.get("items", []):
                t = TOUCHED.get(it["id"], {})
                if "name" in t:
                    it["name"] = t["name"]
                at = t.get("at")
                if at is None:
                    try:
                        at = os.path.getmtime(it.get("full_path") or "")
                    except OSError:
                        at = 0
                it["updated_at"] = utc(at)
                it["name_sort_key"] = name_sort_key(it.get("name"))
            page["items"].sort(key=lambda it: (it["name_sort_key"], it["id"]))
        return status, page

    def everything(self):
        status, ps = self.ask("/api/platforms")
        if status != 200:
            return status, ps
        out = []
        for p in ps:
            status, page = self.listing(p["id"])
            if status != 200:
                return status, page
            out += page.get("items", [])
        return 200, out

    def rom(self, rid):
        status, items = self.everything()
        if status != 200:
            return self.send(status, items)
        it = next((it for it in items if it["id"] == rid), None)
        return self.send(200, it) if it else self.send(404, {"detail": f"Rom with ID {rid} not found"})

    def identifiers(self):
        if self.fault("GET", "/api/roms/identifiers") == "500":
            return self.send(500, {"detail": "Internal Server Error"})
        status, items = self.everything()
        return self.send(status, sorted(it["id"] for it in items) if status == 200 else items)

    def roms(self, qsl):
        q = dict(qsl)
        terms = [t.strip() for t in q.get("search_term", "").split("|") if t.strip()]
        status, page = self.listing(q.get("platform_ids") or q.get("platform_id") or "0", qsl)
        if status != 200:
            return self.send(status, page)
        found = lambda it: any(all(any(w.lower() in (it.get(k) or "").lower() for k in ("name", "fs_name")) for w in t.split()) for t in terms)
        items = [it for it in page["items"] if not terms or found(it)]
        if q.get("updated_after"):
            try:
                after = datetime.datetime.fromisoformat(q["updated_after"].replace("Z", "+00:00"))
            except ValueError:
                return self.send(422, {"detail": [{"type": "datetime_from_date_parsing", "loc": ["query", "updated_after"], "msg": "Input should be a valid datetime"}]})
            if after.tzinfo is None:
                after = after.replace(tzinfo=datetime.timezone.utc)
            items = [it for it in items if datetime.datetime.fromisoformat(it["updated_at"]) > after]
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

    # ---- install requests: RomM 5.4.1's endpoints/device/install.py, its handler's statuses kept in memory
    def installs(self, method, dev, sub, raw):
        s = self.scopes()
        if s is None:
            return self.send(401, {"detail": "Not authenticated"})
        need = {"GET": {"devices.read"}, "PUT": {"devices.write"}}.get(method, {"devices.write", "roms.read"})
        if not need <= s:
            return self.send(403, {"detail": "Forbidden"})
        a = self.headers.get("Authorization", "")
        with lock:
            bound = BOUND.get(a[7:])
        status, d = self.ask(f"/api/devices/{urllib.parse.quote(dev)}")
        if status != 200 or bound not in (None, dev):
            return self.send(404, {"detail": f"Device with ID {dev} not found"})
        now = utc(time.time())
        if method == "GET" and sub is None:
            with lock:
                return self.send(200, [r for r in INSTALLS if r["device_id"] == dev])
        if method == "POST" and sub is None:
            b = json.loads(raw) if raw else {}
            rid = b.get("rom_id")
            if not isinstance(rid, int) or rid < 1:
                return self.send(422, {"detail": [{"type": "int_parsing", "loc": ["body", "rom_id"], "msg": "Input should be a valid integer"}]})
            if (d.get("capabilities") or {}).get("remote_install") is not True:
                return self.send(400, {"detail": f"Device {dev} does not accept remote installs"})
            st, items = self.everything()
            rom = next((it for it in items if it["id"] == rid), None) if st == 200 else None
            if not rom:
                return self.send(404, {"detail": f"Rom with ID {rid} not found"})
            ids = sorted(f["id"] for f in ([] if rom.get("missing_from_fs") else rom.get("files", []))
                         if f.get("category") in (None, "game", "update", "dlc") and not f.get("missing_from_fs"))
            if not ids:
                return self.send(400, {"detail": "This rom has no installable files"})
            with lock:
                live = next((r for r in INSTALLS if r["device_id"] == dev and r["rom_id"] == rid), None)
                if live:
                    return self.send(200, live)
                r = {"id": f"req-{len(INSTALLS) + len(ENDED) + 1}", "user_id": 1, "device_id": dev, "rom_id": rid, "file_ids": ids,
                     "status": "pending", "reason": None, "created_at": now, "updated_at": now}
                INSTALLS.append(r)
            return self.send(201, r)
        if bound != dev:
            return self.send(403, {"detail": "Only the device itself can do this"})
        if method == "POST" and sub == "claim":
            with lock:
                for r in INSTALLS:
                    if r["device_id"] == dev and r["status"] == "pending":
                        r.update(status="taken", updated_at=now)
                return self.send(200, [r for r in INSTALLS if r["device_id"] == dev and r["status"] == "taken"])
        if method == "PUT" and sub not in (None, "claim"):
            b = json.loads(raw) if raw else {}
            if b.get("status") not in DEVICE_REPORTS:
                return self.send(422, {"detail": [{"type": "value_error", "loc": ["body", "status"], "msg": "A device reports one of already_installed, done, failed"}]})
            if b.get("reason") is not None and (not isinstance(b["reason"], str) or len(b["reason"]) > 500):
                return self.send(422, {"detail": [{"type": "string_too_long", "loc": ["body", "reason"], "msg": "String should have at most 500 characters"}]})
            with lock:
                r = next((r for r in INSTALLS if r["id"] == sub and r["device_id"] == dev), None)
                if not r:
                    return self.send(404, {"detail": f"Install request {sub} not found"})
                if r["status"] != "taken":
                    return self.send(409, {"detail": f"Cannot move a {r['status']} request to {b['status']}"})
                INSTALLS.remove(r)
                ended = dict(r, status=b["status"], reason=b.get("reason"), updated_at=now)
                ENDED.append(ended)
            return self.send(200, ended)
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
        if p == "/kiwami/touch":
            with lock:
                TOUCHED[int(b["id"])] = dict({"at": time.time()}, **({"name": b["name"]} if "name" in b else {}))
            return self.send(200, {})
        if p == "/kiwami/expire":
            with lock:
                for r in [r for r in INSTALLS if b.get("id") in (None, r["id"])]:
                    INSTALLS.remove(r)
                    ENDED.append(dict(r, status="expired"))
            return self.send(200, {})
        if p == "/kiwami/installs":
            with lock:
                return self.send(200, {"live": INSTALLS, "ended": ENDED})
        return self.send(404, {"detail": "Not Found"})


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True


srv = Server(("127.0.0.1", 0), H)
with open(PORT_FILE + ".new", "w") as f:
    f.write(f"{srv.server_address[1]}\n")
os.rename(PORT_FILE + ".new", PORT_FILE)
srv.serve_forever()
