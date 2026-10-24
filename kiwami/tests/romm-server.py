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
# and for the tests:
#   POST /test/approve, /test/deny, /test/expire    the newest pairing waiting (or ?user_code=)
#   POST /test/revoke                   every token refused from then on (401)
#   POST /test/version?v=<version>      the heartbeat's version from then on
#   GET  /test/devices                  the devices, one "<id> <name> <client> <client_version> <identifier>" line each
# Its library (platforms, ROMs and their files' bytes, firmware) is kiwami/tests/romm/library.json. Each request
# appends "METHOD path?query auth=<0|1>" to <log file>: whether a token was sent, never the token. With KIWAMI_ROMM_DOWN
# set it closes every connection unanswered, as a server that can't be reached; with KIWAMI_ROMM_STALL=<s> it holds each
# answer that long, as one that stalls.
import http.server
import json
import os
import secrets
import sys
import threading
import time
import urllib.parse
import uuid

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
        with open(self.server.log, "a") as f:
            f.write("%s %s auth=%d\n" % (method, self.path, bool(self.token())))
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
        if path.startswith("/api/devices/"):
            dev = self.authed()
            if dev is None:
                return
            want = path[len("/api/devices/"):]
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
        if method == "POST" and path == "/test/revoke":
            st.tokens.clear()
            return self.reply(200, {"ok": True})
        if method == "POST" and path == "/test/version":
            st.version = q.get("v", st.version)
            return self.reply(200, {"ok": True})
        if method == "GET" and path == "/test/devices":
            lines = "".join("%s %s %s %s %s\n" % (i, d["name"].replace(" ", "_"), d["client"], d["client_version"],
                                                d["identifier"]) for i, d in st.devices.items())
            return self.reply(200, lines.encode(), "text/plain")
        self.detail(404, "Not Found")

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
