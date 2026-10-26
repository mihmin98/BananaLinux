#!/usr/bin/env python3
# A stand-in for a RomM 5.4.1 server (tag 9c74d01), for the fork's tests of the RomM helper and of Settings' RomM
# pages (accounts.conf's romm_url pointing at it):
#   kiwami/tests/romm-server.py <port file> <log file>
# It listens on 127.0.0.1, on a free port it writes to <port file> once it is ready. Its answers follow RomM's own
# source, not its docs:
#   GET  /api/heartbeat                 SYSTEM.VERSION: KIWAMI_ROMM_VERSION (5.4.1 by default)
#   POST /api/auth/device/init          the device flow's start (backend/endpoints/device/auth.py): its payload checked
#                                       as RomM's pydantic model does (422), 201 with the codes; its interval is
#                                       KIWAMI_ROMM_INTERVAL (1 s by default, RomM's is 5) so the tests run quickly
#   POST /api/auth/device/token         400 authorization_pending until the test approves, denies or expires it;
#                                       slow_down when polled faster than the interval; then the token (rmm_...) and
#                                       the device once, as RomM's consume_approved
#   GET  /api/devices/<id>              the device (devices.read), 404 when it isn't the token's user's
#   DELETE /api/devices/<id>            204, the device and its tokens gone
#   GET  /api/platforms                 the library's platforms, with their games' counts
#   GET  /api/roms                      a platform's games (platform_ids, search_term, offset, limit, order_by id or
#                                       name), paged as RomM's {items, total, limit, offset}
#   GET  /api/roms/<id>                 a game, with its files (paths in its folder) and how RomM keeps it: one file
#                                       (fs_name the file's), one file in a folder of its own, or several files
#   GET  /api/roms/<id>/content/<name>  a game's bytes as endpoints/roms/__init__.py's get_rom_content sends them: one
#                                       file as itself, several as a zip (stored) with <name>.m3u added when they have
#                                       none; a Range "bytes=<n>-" answered 206 from there (RomM's nginx). For several
#                                       files, a Range gets RomM's cached zip and none gets mod_zip's stream, another
#                                       zip of the same files (here: with data descriptors), as RomM does
#   GET  /api/firmware                  the firmware (platform_id), as endpoints/firmware.py's FirmwareSchema
#   GET  /api/firmware/<id>/content/<name>      a firmware file's bytes, Range as a game's
#   POST /api/sync/negotiate            the saves' negotiation as endpoints/sync/__init__.py's negotiate_sync and
#                                       handler/sync/comparison.py decide it: saves paired on (rom_id, slot), the
#                                       newest of a slot the current one, each device's last sync of a save (its time,
#                                       the client's and the server's hashes), versions a slot lost and when (delete),
#                                       scoped by rom_ids
#   POST /api/saves                     an upload (multipart saveFile) with endpoints/saves.py's add_save rules: 409
#                                       when the slot's newest save isn't the one this device last synced (unless
#                                       overwrite), the same bytes again in a slot taken as that save
#   GET  /api/saves/<id>/content        a save's bytes, the device's sync of it recorded (its server half) unless
#                                       optimistic=false
#   POST /api/saves/<id>/downloaded     the device's sync completed with the hash it holds, as confirm_download
#   POST /api/sync/sessions/<id>/complete    the session closed, its play sessions kept
#   POST /api/play-sessions             play sessions on their own (201), one with the same game and start skipped
# and for the tests:
#   POST /test/approve, /test/deny, /test/expire    the newest pairing waiting (or ?user_code=)
#   POST /test/revoke                   every token refused from then on (401)
#   POST /test/version?v=<version>      the heartbeat's version from then on
#   GET  /test/devices                  the devices, one "<id> <name> <client> <client_version> <identifier>" line each
#   POST /test/save?rom_id=&slot=&emulator=     the body uploaded as another device would (RomM's web player);
#                                       &zip=1 kept as grout keeps a save, a zip of web.srm (the body) and web.rtc
#                                       ("rtc:" and the body), hashed as RomM hashes a zip; &zip=2 a zip of two saves;
#                                       &corrupt=1 kept with a hash that isn't its bytes'
#   POST /test/delete-slot?rom_id=&slot=        the slot emptied, as RomM's own page does (its versions' removal kept)
#   GET  /test/saves                    the saves, as JSON: id, rom_id, slot, emulator, hash, size, file_name, data
#                                       (the bytes, latin-1), newest last
#   GET  /test/play-sessions            the play sessions received, as JSON
#   POST /test/cut?bytes=<n>            each download from then on closed after n bytes of its body (0: whole)
#   POST /test/ranges?on=0|1            a Range ignored from then on (the whole file again, 200), or answered
#   POST /test/slow?ms=<n>              each download from then on held n ms before it starts (0: at once)
# Its library (platforms, ROMs and their files' bytes, firmware) is kiwami/tests/romm/library.json. Each request
# appends "METHOD path?query auth=<0|1>" to <log file>: whether a token was sent, never the token; and " range=<Range>"
# when it has one. With KIWAMI_ROMM_DOWN
# set it closes every connection unanswered, as a server that can't be reached; with KIWAMI_ROMM_STALL=<s> it holds each
# answer that long, as one that stalls.
import datetime
import hashlib
import http.server
import io
import json
import os
import secrets
import sys
import threading
import time
import urllib.parse
import uuid
import zipfile

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "romm")
DOWN = bool(os.environ.get("KIWAMI_ROMM_DOWN"))
STALL = float(os.environ.get("KIWAMI_ROMM_STALL", "0") or 0)
INTERVAL = int(os.environ.get("KIWAMI_ROMM_INTERVAL", "1"))
# backend/handler/auth/constants.py's Scope
SCOPES = {"me.read", "me.write", "roms.read", "roms.write", "roms.user.read", "roms.user.write", "platforms.read",
          "platforms.write", "assets.read", "assets.write", "devices.read", "devices.write", "firmware.read",
          "firmware.write", "collections.read", "collections.write", "playlists.read", "playlists.write", "users.read",
          "users.write", "tasks.run", "logs.read"}
PAIR_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.version = os.environ.get("KIWAMI_ROMM_VERSION", "5.4.1")
        self.flows = {}      # device_code -> {user_code, status, payload, polled, token, device_id}
        self.order = []      # device codes, oldest first
        self.devices = {}    # id -> {name, client, client_version, identifier, scopes}
        self.tokens = {}     # token -> device id
        with open(os.path.join(HERE, "library.json")) as f:
            self.library = json.load(f)
        for fl in self.library.get("filler", []):
            for i in range(1, fl["count"] + 1):
                name = fl["name"] % i
                self.library["roms"].append({"id": fl["base"] + i, "platform_id": fl["platform_id"], "name": name,
                                             "fs_name": name + "." + fl["ext"],
                                             "files": [{"file_name": name + "." + fl["ext"], "size": fl["size"],
                                                        "seed": fl["base"] + i}]})
        self.saves = []      # {id, rom_id, slot, emulator, file_name, hash, updated, data}, oldest first
        self.syncs = {}      # (device, save id) -> {synced, hash, server_hash}
        self.removed = {}    # (rom_id, slot) -> {hash: when}
        self.sessions = {}   # id -> {device, status}
        self.plays = []      # the play sessions received
        self.next_id = 1
        self.cut = 0         # downloads closed after this many bytes (0: whole)
        self.ranges = True   # a Range answered (206)
        self.slow = 0        # downloads held this many ms first

    def new_id(self):
        self.next_id += 1
        return self.next_id

    # the current save of each (rom_id, slot): the newest
    def current(self, rom_ids=None):
        cur = {}
        for sv in self.saves:
            if sv["slot"] is None or (rom_ids is not None and sv["rom_id"] not in rom_ids):
                continue
            k = (sv["rom_id"], sv["slot"])
            if k not in cur or sv["updated"] > cur[k]["updated"]:
                cur[k] = sv
        return cur

    def save_by_id(self, i):
        return next((sv for sv in self.saves if sv["id"] == i), None)


# a library file's bytes, made up from its seed: the same every time, so a download resumes into the same file
def file_bytes(f):
    out, i = bytearray(), 0
    while len(out) < f["size"]:
        out += hashlib.sha256(b"%d:%d" % (f["seed"], i)).digest()
        i += 1
    return bytes(out[:f["size"]])


# one file kept as itself (fs_name its name), not in a folder of its own
def simple(r):
    return len(r["files"]) == 1 and r["files"][0]["file_name"] == r["fs_name"]


class Stream(io.RawIOBase):
    """Bytes written where they can't be sought back to, as mod_zip streams: zipfile then writes data descriptors."""
    def __init__(self):
        self.data = bytearray()

    def writable(self):
        return True

    def write(self, b):
        self.data += b
        return len(b)


# several files as get_rom_content zips them: each by its path in the game's folder, and <name>.m3u listing them when
# the game has none; stored, at a fixed time, so the same zip each time. streamed: mod_zip's (no Range), another zip
# of the same files than the cached one a Range gets
def rom_zip(r, name, streamed=False):
    buf = Stream() if streamed else io.BytesIO()
    files = sorted(r["files"], key=lambda f: f["file_name"])
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_STORED) as z:
        def put(n, data):
            zi = zipfile.ZipInfo(n, (2026, 10, 1, 0, 0, 0))
            zi.external_attr = 0o100600 << 16
            z.writestr(zi, data)
        for f in files:
            put(f["file_name"], file_bytes(f))
        if not any(f["file_name"].lower().endswith(".m3u") for f in files):
            put(name + ".m3u", "\n".join(f["file_name"] for f in files).encode())
    return bytes(buf.data) if streamed else buf.getvalue()


# assets_handler.py's hash_zip_contents: md5 of "<name>:<md5>" lines, by name, folders left out
def zip_hash(data):
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        return hashlib.md5("\n".join("%s:%s" % (n, hashlib.md5(z.read(n)).hexdigest())
                                     for n in sorted(z.namelist()) if not n.endswith("/")).encode()).hexdigest()


def zipped(files):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        for n, data in files:
            z.writestr(n, data)
    return buf.getvalue()


def parse_time(text):
    return datetime.datetime.fromisoformat(text.replace("Z", "+00:00")).timestamp()


def iso(t):
    return datetime.datetime.fromtimestamp(t, datetime.timezone.utc).isoformat()


# handler/sync/comparison.py, with times as seconds
def held_since_removal(client_hash, client_ts, removed):
    when = (removed or {}).get(client_hash) if client_hash else None
    return when is not None and client_ts <= when


def compare_save_state(client_hash, client_ts, server_hash, server_ts, sync, removed):
    if client_hash and server_hash and client_hash == server_hash:
        return "no_op", "Content is identical"
    if held_since_removal(client_hash, client_ts, removed):
        return "download", "Client holds a version removed on the server"
    if sync:
        client_unchanged = bool(client_hash and sync["hash"] and client_hash == sync["hash"])
        server_unchanged = bool(server_hash and sync["server_hash"] and server_hash == sync["server_hash"])
        client_changed = client_ts > sync["synced"] and not client_unchanged
        server_changed = server_ts > sync["synced"] and not server_unchanged
        if client_changed and server_changed:
            return "conflict", "Both sides changed since last sync"
        if client_changed:
            return "upload", "Client save is newer than last sync"
        if server_changed:
            return "download", "Server save is newer than last sync"
        return "no_op", "No changes since last sync"
    if client_ts > server_ts:
        return "upload", "Client save is newer (no sync history)"
    if server_ts > client_ts:
        return "download", "Server save is newer (no sync history)"
    if client_hash != server_hash:
        return "conflict", "Same timestamp but different content"
    return "no_op", "Saves appear identical"


def multipart_file(ctype, body):
    """The first file part of a multipart/form-data body: (field, filename, bytes)."""
    boundary = ctype.split("boundary=", 1)[1].strip().strip('"').encode()
    for part in body.split(b"--" + boundary)[1:]:
        if part.startswith(b"--"):
            break
        head, _, data = part.partition(b"\r\n\r\n")
        head = head.decode("latin-1")
        if data.endswith(b"\r\n"):
            data = data[:-2]
        name = head.split('name="', 1)[1].split('"', 1)[0] if 'name="' in head else ""
        fname = head.split('filename="', 1)[1].split('"', 1)[0] if 'filename="' in head else ""
        return name, fname, data
    return "", "", b""


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def reply(self, status, body=b"", kind="application/json"):
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode()
        self.send_response(status)
        if body or status != 204:
            self.send_header("Content-Type", kind)
            self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def detail(self, status, text):
        self.reply(status, {"detail": text})

    def drop(self):
        self.close_connection = True
        self.wfile.flush()
        self.connection.shutdown(2)

    def token(self):
        a = self.headers.get("Authorization", "")
        return a[7:] if a.startswith("Bearer ") else ""

    # the token's device, or None after a 401 sent
    def authed(self):
        st = self.server.state
        dev = st.tokens.get(self.token())
        if dev is None:
            self.detail(401, "Invalid or expired token")
        return dev

    def body(self):
        n = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(n) if n else b""

    def handle_one(self, method):
        if DOWN:
            return self.drop()
        u = urllib.parse.urlsplit(self.path)
        q = {k: v[0] for k, v in urllib.parse.parse_qs(u.query, keep_blank_values=True).items()}
        rng = self.headers.get("Range")
        with open(self.server.log, "a") as f:
            f.write("%s %s auth=%d%s\n" % (method, self.path, bool(self.token()), " range=" + rng if rng else ""))
        if STALL and not u.path.startswith("/test/"):
            time.sleep(STALL)
        st = self.server.state
        with st.lock:
            return self.route(method, u.path, q, st)

    def route(self, method, path, q, st):
        if method == "GET" and path == "/api/heartbeat":
            return self.reply(200, {"SYSTEM": {"VERSION": st.version, "GIT_BRANCH": None, "SHOW_SETUP_WIZARD": False},
                                    "METADATA_SOURCES": {}, "FILESYSTEM": {}})
        if method == "POST" and path == "/api/auth/device/init":
            return self.init(st)
        if method == "POST" and path == "/api/auth/device/token":
            return self.poll(st)
        if path.startswith("/api/") and path != "/api/heartbeat" and not path.startswith("/api/auth/"):
            dev = self.authed()
            if dev is None:
                return
            return self.api(method, path, q, st, dev)
        if method == "POST" and path in ("/test/approve", "/test/deny", "/test/expire"):
            code = self.waiting(st, q.get("user_code"))
            if code is None:
                return self.detail(404, "no pairing waiting")
            flow = st.flows[code]
            if path == "/test/approve":
                dev = str(uuid.uuid4())
                p = flow["payload"]
                st.devices[dev] = {"name": p["name"], "client": p["client"], "client_version": p.get("client_version"),
                                   "identifier": p["client_device_identifier"], "scopes": p["requested_scopes"]}
                flow.update(status="approved", token="rmm_" + secrets.token_hex(16), device_id=dev)
            elif path == "/test/deny":
                flow["status"] = "denied"
            else:
                del st.flows[code]
            return self.reply(200, {"ok": True})
        if method == "POST" and path == "/test/save":
            data, name = self.body(), "web.sav"
            rom = int(q["rom_id"])
            if q.get("zip") == "1":
                data, name = zipped([("web.srm", data), ("web.rtc", b"rtc:" + data)]), "web.zip"
            elif q.get("zip") == "2":
                data, name = zipped([("web.srm", data), ("other.srm", data)]), "web.zip"
            h = zip_hash(data) if q.get("zip") else hashlib.md5(data).hexdigest()
            if q.get("corrupt") == "1":
                h = hashlib.md5(data + b"x").hexdigest()
            st.saves.append({"id": st.new_id(), "rom_id": rom, "slot": q.get("slot", "autosave"),
                             "emulator": q.get("emulator"), "file_name": name, "hash": h,
                             "updated": time.time(), "data": data})
            return self.reply(200, {"id": st.next_id})
        if method == "POST" and path == "/test/delete-slot":
            key = (int(q["rom_id"]), q.get("slot", "autosave"))
            gone = [sv for sv in st.saves if (sv["rom_id"], sv["slot"]) == key]
            for sv in gone:
                st.removed.setdefault(key, {})[sv["hash"]] = time.time()
                st.saves.remove(sv)
            return self.reply(200, {"deleted": len(gone)})
        if method == "GET" and path == "/test/saves":
            return self.reply(200, [{"id": sv["id"], "rom_id": sv["rom_id"], "slot": sv["slot"], "emulator": sv["emulator"],
                                     "hash": sv["hash"], "size": len(sv["data"]), "file_name": sv["file_name"],
                                     "data": sv["data"].decode("latin-1")} for sv in st.saves])
        if method == "GET" and path == "/test/play-sessions":
            return self.reply(200, st.plays)
        if method == "POST" and path == "/test/revoke":
            st.tokens.clear()
            return self.reply(200, {"ok": True})
        if method == "POST" and path == "/test/cut":
            st.cut = int(q.get("bytes", "0"))
            return self.reply(200, {"ok": True})
        if method == "POST" and path == "/test/ranges":
            st.ranges = q.get("on", "1") == "1"
            return self.reply(200, {"ok": True})
        if method == "POST" and path == "/test/slow":
            st.slow = int(q.get("ms", "0"))
            return self.reply(200, {"ok": True})
        if method == "POST" and path == "/test/version":
            st.version = q.get("v", st.version)
            return self.reply(200, {"ok": True})
        if method == "GET" and path == "/test/devices":
            lines = "".join("%s %s %s %s %s\n" % (i, d["name"].replace(" ", "_"), d["client"], d["client_version"],
                                                d["identifier"]) for i, d in st.devices.items())
            return self.reply(200, lines.encode(), "text/plain")
        self.detail(404, "Not Found")

    # the API behind a token: dev is the token's device
    def api(self, method, path, q, st, dev):
        parts = path.strip("/").split("/")
        if parts[:2] == ["api", "devices"] and len(parts) == 3:
            want = parts[2]
            if want != dev or want not in st.devices:
                return self.detail(404, "Device with ID %s not found" % want)
            if method == "GET":
                d = st.devices[want]
                return self.reply(200, {"id": want, "name": d["name"], "client": d["client"],
                                        "client_version": d["client_version"], "sync_enabled": True})
            if method == "DELETE":
                del st.devices[want]
                for t in [t for t, d in st.tokens.items() if d == want]:
                    del st.tokens[t]
                return self.reply(204)
        lib = st.library
        if method == "GET" and path == "/api/platforms":
            return self.reply(200, [{"id": p["id"], "slug": p["slug"], "fs_slug": p["slug"], "name": p["name"],
                                     "rom_count": sum(1 for r in lib["roms"] if r["platform_id"] == p["id"])}
                                    for p in lib["platforms"]])
        if method == "GET" and path == "/api/roms":
            pids = {int(x) for x in urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query).get("platform_ids", [])}
            items = [r for r in lib["roms"] if not pids or r["platform_id"] in pids]
            term = q.get("search_term", "").lower()
            if term:
                items = [r for r in items if all(w in (r["name"] + " " + r["fs_name"]).lower() for w in term.split())]
            if q.get("order_by") == "name":
                items.sort(key=lambda r: (r["name"].lower(), r["id"]))
            else:
                items.sort(key=lambda r: r["id"])
            offset, limit = int(q.get("offset", "0")), int(q.get("limit", "50"))
            if not 1 <= limit <= 10000:
                return self.detail(422, "limit out of range")
            page = items[offset:offset + limit]
            # (RomM's SimpleRomSchema has some 90 fields; the ones the helper reads, and a summary as heavy as RomM's)
            return self.reply(200, {"items": [dict(self.rom_fields(r), summary="x" * 400) for r in page],
                                    "total": len(items), "limit": limit, "offset": offset})
        if parts[:2] == ["api", "roms"] and len(parts) in (3, 5) and method == "GET":
            r = next((r for r in lib["roms"] if str(r["id"]) == parts[2]), None)
            if r is None:
                return self.detail(404, "Rom with ID %s not found" % parts[2])
            if len(parts) == 3:
                plat = next(p for p in lib["platforms"] if p["id"] == r["platform_id"])
                folder = plat["slug"] + "/" + r["fs_name"]
                return self.reply(200, dict(self.rom_fields(r), full_path=folder,
                                            files=[{"id": r["id"] * 10 + i, "rom_id": r["id"], "file_name": f["file_name"],
                                                    "file_path": folder, "file_size_bytes": f["size"],
                                                    "full_path": (plat["slug"] if simple(r) else folder) + "/" + f["file_name"],
                                                    "is_top_level": True} for i, f in enumerate(r["files"])]))
            if parts[3] != "content":
                return self.detail(404, "Not Found")
            if len(r["files"]) == 1:
                return self.content(st, file_bytes(r["files"][0]), "application/octet-stream")
            cached = st.ranges and self.headers.get("Range", "").startswith("bytes=")
            return self.content(st, rom_zip(r, urllib.parse.unquote(parts[4]), not cached), "application/zip")
        if method == "GET" and path == "/api/firmware":
            pid = int(q["platform_id"]) if q.get("platform_id") else None
            return self.reply(200, [{"id": fw["id"], "platform_id": fw["platform_id"], "file_name": fw["file_name"],
                                     "file_size_bytes": fw["size"], "md5_hash": hashlib.md5(file_bytes(fw)).hexdigest(),
                                     "missing_from_fs": False}
                                    for fw in lib["firmware"] if pid is None or fw["platform_id"] == pid])
        if parts[:2] == ["api", "firmware"] and len(parts) == 5 and parts[3] == "content" and method == "GET":
            fw = next((fw for fw in lib["firmware"] if str(fw["id"]) == parts[2]), None)
            if fw is None:
                return self.detail(404, "Firmware with ID %s not found" % parts[2])
            return self.content(st, file_bytes(fw), "application/octet-stream")
        if method == "POST" and path == "/api/sync/negotiate":
            return self.negotiate(st, dev)
        if method == "POST" and path == "/api/saves":
            return self.upload(st, dev, q)
        if parts[:2] == ["api", "saves"] and len(parts) == 4 and parts[3] == "content" and method == "GET":
            sv = st.save_by_id(int(parts[2]))
            if sv is None:
                return self.detail(404, "Save with ID %s not found" % parts[2])
            if q.get("device_id") == dev and q.get("optimistic", "true") != "false":
                st.syncs[(dev, sv["id"])] = {"synced": sv["updated"], "hash": None, "server_hash": sv["hash"]}
            return self.reply(200, sv["data"], "application/octet-stream")
        if parts[:2] == ["api", "saves"] and len(parts) == 4 and parts[3] == "downloaded" and method == "POST":
            sv = st.save_by_id(int(parts[2]))
            if sv is None:
                return self.detail(404, "Save with ID %s not found" % parts[2])
            p = json.loads(self.body() or b"{}")
            synced, server_hash, client_hash = sv["updated"], sv["hash"], p.get("content_hash")
            served = st.syncs.get((p["device_id"], sv["id"]))
            if served and served["server_hash"] and not served["hash"]:
                synced, server_hash = served["synced"], served["server_hash"]
            elif client_hash != server_hash:
                client_hash = server_hash = None
            st.syncs[(p["device_id"], sv["id"])] = {"synced": synced, "hash": client_hash, "server_hash": server_hash}
            return self.reply(200, {"id": sv["id"]})
        if parts[:3] == ["api", "sync", "sessions"] and len(parts) == 5 and parts[4] == "complete" and method == "POST":
            sess = st.sessions.get(int(parts[3]))
            if sess is None:
                return self.detail(404, "Sync session with ID %s not found" % parts[3])
            if sess["status"] != "in_progress":
                return self.detail(400, "Session is already %s" % sess["status"])
            p = json.loads(self.body() or b"{}")
            if not self.take_plays(st, dev, p.get("play_sessions") or []):
                return
            sess["status"] = "completed"
            return self.reply(200, {"session": {"id": int(parts[3]), "status": "completed"}})
        if method == "POST" and path == "/api/play-sessions":
            p = json.loads(self.body() or b"{}")
            if not p.get("sessions"):
                return self.detail(400, "Payload must contain at least one session")
            if not self.take_plays(st, dev, p["sessions"]):
                return
            return self.reply(201, {"results": [], "created_count": len(p["sessions"]), "skipped_count": 0})
        self.detail(404, "Not Found")

    # a game's fields as SimpleRomSchema has them (of its some 90, the ones the helper reads)
    def rom_fields(self, r):
        plat = next(p for p in self.server.state.library["platforms"] if p["id"] == r["platform_id"])
        return {"id": r["id"], "platform_id": r["platform_id"], "platform_slug": plat["slug"], "fs_name": r["fs_name"],
                "name": r["name"], "fs_size_bytes": sum(f["size"] for f in r["files"]),
                "has_simple_single_file": simple(r), "has_nested_single_file": len(r["files"]) == 1 and not simple(r),
                "has_multiple_files": len(r["files"]) > 1}

    # a file's bytes: from a Range's start (206, from 0 too), unless ranges are off; held first by /test/slow, cut short
    # after /test/cut's bytes
    def content(self, st, data, kind):
        time.sleep(st.slow / 1000)
        start, rng = None, self.headers.get("Range", "")
        if st.ranges and rng.startswith("bytes=") and rng.endswith("-"):
            start = int(rng[6:-1])
            if start >= len(data):
                return self.detail(416, "Range Not Satisfiable")
        body = data[start or 0:]
        self.send_response(200 if start is None else 206)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        if start is not None:
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, len(data) - 1, len(data)))
        self.end_headers()
        if st.cut and st.cut < len(body):
            self.wfile.write(body[:st.cut])
            return self.drop()
        self.wfile.write(body)

    # PlaySessionEntry's rules (end after start, to the second), then kept, one with the same game and start skipped
    def take_plays(self, st, dev, plays):
        for e in plays:
            if int(parse_time(e["end_time"])) <= int(parse_time(e["start_time"])) or e.get("duration_ms", -1) < 0:
                self.detail(422, "end_time must be after start_time")
                return False
        for e in plays:
            if not any(x["rom_id"] == e.get("rom_id") and x["start_time"] == e["start_time"] for x in st.plays):
                st.plays.append(dict(e, device_id=dev))
        return True

    def negotiate(self, st, dev):
        p = json.loads(self.body() or b"{}")
        saves = p.get("saves", [])
        rom_ids = p.get("rom_ids")
        if rom_ids is not None and len(rom_ids) > 500:
            return self.detail(422, "too many rom_ids")
        scope = set(rom_ids) | {c["rom_id"] for c in saves} if rom_ids is not None else None
        cur = st.current(scope)
        sid = st.new_id()
        st.sessions[sid] = {"device": dev, "status": "in_progress"}
        ops, matched = [], set()

        def op(action, reason, rom, sv=None, client=None):
            ops.append({"action": action, "rom_id": rom, "save_id": sv["id"] if sv else None,
                        "file_name": sv["file_name"] if sv else client["file_name"],
                        "slot": sv["slot"] if sv else client.get("slot"),
                        "emulator": sv["emulator"] if sv else client.get("emulator"), "reason": reason,
                        "server_updated_at": iso(sv["updated"]) if sv else None,
                        "server_content_hash": sv["hash"] if sv else None})

        for c in saves:
            key = (c["rom_id"], c.get("slot"))
            sv = cur.get(key) if c.get("slot") else None
            removed = st.removed.get(key, {})
            cts = parse_time(c["updated_at"])
            if sv is None:
                gone = held_since_removal(c.get("content_hash"), cts, removed)
                op("delete" if gone else "upload", "Save was deleted on the server" if gone else
                   "Save exists on client but not on server", c["rom_id"], client=c)
                continue
            matched.add(sv["id"])
            sync = st.syncs.get((dev, sv["id"]))
            action, reason = compare_save_state(c.get("content_hash"), cts, sv["hash"], sv["updated"], sync, removed)
            if action == "no_op" and c.get("content_hash") == sv["hash"]:
                old = st.syncs.get((dev, sv["id"]))
                if not old or old["hash"] != sv["hash"]:
                    st.syncs[(dev, sv["id"])] = {"synced": sv["updated"], "hash": sv["hash"], "server_hash": sv["hash"]}
            op(action, reason, sv["rom_id"], sv)
        for sv in cur.values():
            if sv["id"] in matched:
                continue
            sync = st.syncs.get((dev, sv["id"]))
            if sync and sv["updated"] <= sync["synced"] and not p.get("restore_unlisted"):
                continue
            op("download", "Save exists on server but not on client", sv["rom_id"], sv)
        count = lambda a: sum(1 for o in ops if o["action"] == a)
        self.reply(200, {"session_id": sid, "operations": ops, "total_upload": count("upload"),
                         "total_download": count("download"), "total_conflict": count("conflict"),
                         "total_no_op": count("no_op"), "total_delete": count("delete")})

    def upload(self, st, dev, q):
        ctype = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in ctype:
            return self.detail(422, "saveFile missing")
        field, fname, data = multipart_file(ctype, self.body())
        if field != "saveFile" or not fname:
            return self.detail(400, "Save file has no filename")
        rom = int(q["rom_id"])
        if not any(r["id"] == rom for r in st.library["roms"]):
            return self.detail(404, "Rom %d not found" % rom)
        slot, device, overwrite = q.get("slot"), q.get("device_id"), q.get("overwrite") == "true"
        if device and device != dev:
            return self.detail(404, "Device with ID %s not found" % device)
        if device and slot and not overwrite:
            in_slot = [sv for sv in st.saves if sv["rom_id"] == rom and sv["slot"] == slot]
            if in_slot:
                latest = max(in_slot, key=lambda sv: sv["updated"])
                sync = st.syncs.get((device, latest["id"]))
                if not sync or sync["synced"] < latest["updated"]:
                    return self.detail(409, "Slot has a newer save since your last sync")
        h = hashlib.md5(data).hexdigest()
        if slot and not overwrite:
            same = next((sv for sv in st.saves if sv["rom_id"] == rom and sv["slot"] == slot and sv["hash"] == h), None)
            if same:
                if device:
                    st.syncs[(device, same["id"])] = {"synced": same["updated"], "hash": q.get("content_hash"),
                                                      "server_hash": same["hash"]}
                return self.reply(200, {"id": same["id"], "content_hash": same["hash"]})
        # a slot's uploads are each a version of their own, the newest the slot's
        sv = {"id": st.new_id(), "rom_id": rom, "slot": slot, "emulator": q.get("emulator"), "file_name": fname,
              "hash": h, "updated": time.time(), "data": data}
        st.saves.append(sv)
        if device:
            st.syncs[(device, sv["id"])] = {"synced": sv["updated"], "hash": q.get("content_hash"), "server_hash": h}
        self.reply(200, {"id": sv["id"], "content_hash": h, "file_name": fname})

    def waiting(self, st, user_code):
        for code in reversed(st.order):
            f = st.flows.get(code)
            if f and f["status"] == "pending" and (not user_code or f["user_code"] == user_code):
                return code
        return None

    # DeviceAuthInitPayload's rules, then device_auth_init
    def init(self, st):
        try:
            p = json.loads(self.body() or b"null")
        except ValueError:
            return self.detail(422, "invalid JSON")
        ok = isinstance(p, dict)
        for k, top in (("client_device_identifier", 255), ("name", 255), ("client", 50)):
            ok = ok and isinstance(p.get(k), str) and 1 <= len(p[k]) <= top
        for k in ("platform", "client_version"):
            ok = ok and (p.get(k) is None or (isinstance(p[k], str) and len(p[k]) <= 50))
        sc = p.get("requested_scopes") if ok else None
        ok = ok and isinstance(sc, list) and 1 <= len(sc) <= len(SCOPES) and all(s in SCOPES for s in sc)
        if not ok:
            return self.detail(422, "invalid payload")
        code = secrets.token_hex(32)
        user = "".join(secrets.choice(PAIR_ALPHABET) for _ in range(8))
        st.flows[code] = {"user_code": user, "status": "pending", "payload": p, "polled": 0}
        st.order.append(code)
        self.reply(201, {"device_code": code, "user_code": user, "verification_path": "/pair/device",
                         "verification_path_complete": "/pair/device?user_code=" + user, "expires_in": 600,
                         "interval": INTERVAL})

    # token(): pending (slow_down when polled too fast, as polled_too_fast), denied, approved once
    def poll(self, st):
        try:
            code = json.loads(self.body() or b"null").get("device_code", "")
        except (ValueError, AttributeError):
            return self.detail(422, "invalid payload")
        flow = st.flows.get(code)
        if flow is None:
            return self.detail(400, "expired_token")
        if flow["status"] == "pending":
            now = time.monotonic()
            fast = flow["polled"] and now - flow["polled"] < INTERVAL - 0.05
            flow["polled"] = now
            return self.detail(400, "slow_down" if fast else "authorization_pending")
        if flow["status"] == "denied":
            return self.detail(400, "access_denied")
        del st.flows[code]
        st.tokens[flow["token"]] = flow["device_id"]
        self.reply(200, {"access_token": flow["token"], "device_id": flow["device_id"],
                         "scopes": sorted(flow["payload"]["requested_scopes"]), "expires_at": None})

    def do_GET(self):
        self.handle_one("GET")

    def do_POST(self):
        self.handle_one("POST")

    def do_DELETE(self):
        self.handle_one("DELETE")

    def do_PUT(self):
        self.handle_one("PUT")


def main():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.log = sys.argv[2]
    server.state = State()
    open(server.log, "a").close()
    with open(sys.argv[1] + ".part", "w") as f:
        f.write("%d\n" % server.server_address[1])
    os.rename(sys.argv[1] + ".part", sys.argv[1])
    server.serve_forever()


if __name__ == "__main__":
    main()
