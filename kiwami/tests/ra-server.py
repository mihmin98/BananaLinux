#!/usr/bin/env python3
# A stand-in for retroachievements.org and media.retroachievements.org, for the fork's tests of Settings'
# RetroAchievements pages (banana-ui with KIWAMI_RA_URL and KIWAMI_RA_MEDIA set to it):
#   kiwami/tests/ra-server.py <port file> <log file>
# It listens on 127.0.0.1, on a free port it writes to <port file> once it is ready. It answers:
#   POST /dorequest.php     by its r, from kiwami/tests/ra/<r>.json: the most specific of <r>-<k><v>... files for the
#                           request's c, g, m and h (unlocks-g1446-h1, unlocks-h1, then unlocks), else <r>.json.
#                           login2 knows alice (password pw1, token tok1) and bob (pw2, tok2), and refuses anything
#                           else with 401 (login2-refused.json); every other r needs one of their tokens (401,
#                           token-refused.json). login2.json's @USER@, @TOKEN@ and @MEDIA@ are filled in
#   GET /UserPic/<name>.png  ra/avatar.png
#   GET /Badge/<name>.png    ra/badge.png
# and appends a line a request to <log file>: r=<r> u=<u> p=<1|0> t=<1|0>, then g, m, c and h when sent (a password
# or a token is never written, only whether it was sent), or GET <path>. With KIWAMI_RA_SERVER_DOWN set in its
# environment it closes every connection unanswered, as a server that can't be reached.
import http.server
import itertools
import json
import os
import sys
import urllib.parse

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ra")
USERS = {"alice": ("pw1", "tok1"), "bob": ("pw2", "tok2")}
DOWN = bool(os.environ.get("KIWAMI_RA_SERVER_DOWN"))


def canned(name):
    with open(os.path.join(HERE, name), "rb") as f:
        return f.read()


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def record(self, line):
        with open(self.server.log, "a") as f:
            f.write(line + "\n")

    def reply(self, status, body, kind="application/json"):
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def drop(self):
        self.close_connection = True
        self.wfile.flush()
        self.connection.shutdown(2)

    def do_GET(self):
        if DOWN:
            return self.drop()
        self.record("GET " + self.path)
        for prefix, picture in (("/UserPic/", "avatar.png"), ("/Badge/", "badge.png")):
            if self.path.startswith(prefix) and self.path.endswith(".png"):
                return self.reply(200, canned(picture), "image/png")
        self.reply(404, b"not found", "text/plain")

    def do_POST(self):
        if DOWN:
            return self.drop()
        if self.path != "/dorequest.php":
            return self.reply(404, b"not found", "text/plain")
        length = int(self.headers.get("Content-Length", "0"))
        q = {k: v[0] for k, v in urllib.parse.parse_qs(self.rfile.read(length).decode(), keep_blank_values=True).items()}
        r, u = q.get("r", ""), q.get("u", "")
        line = "r=%s u=%s p=%d t=%d" % (r, u, "p" in q, "t" in q)
        line += "".join(" %s=%s" % (k, q[k]) for k in "gmch" if k in q)
        self.record(line)
        known = USERS.get(u.lower())
        if r == "login2":
            if not known or (q.get("p") != known[0] and q.get("t") != known[1]):
                return self.reply(401, canned("login2-refused.json"))
            body = canned("login2.json").decode()
            for k, v in (("@USER@", u), ("@TOKEN@", known[1]), ("@MEDIA@", self.server.media)):
                body = body.replace(k, v)
            return self.reply(200, body.encode())
        if r != "gameid" and (not known or q.get("t") != known[1]):
            return self.reply(401, canned("token-refused.json"))
        # the most specific canned reply: every subset of the request's c, g, m and h, the largest first
        keys = [k + q[k] for k in "cgmh" if k in q]
        for n in range(len(keys), -1, -1):
            for subset in itertools.combinations(keys, n):
                name = "-".join((r,) + subset) + ".json"
                if os.path.isfile(os.path.join(HERE, name)):
                    return self.reply(200, canned(name))
        self.reply(200, json.dumps({"Success": False, "Error": "Unknown request %s" % r}).encode())


def main():
    server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    server.log = sys.argv[2]
    server.media = "http://127.0.0.1:%d" % server.server_address[1]
    open(server.log, "a").close()
    with open(sys.argv[1] + ".part", "w") as f:
        f.write("%d\n" % server.server_address[1])
    os.rename(sys.argv[1] + ".part", sys.argv[1])
    server.serve_forever()


if __name__ == "__main__":
    main()
