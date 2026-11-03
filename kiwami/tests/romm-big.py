#!/usr/bin/env python3
# A stand-in RomM 5.4.1 with a large made-up library, for the fork's test of banana-sync's memory (romm-list.sh):
#   kiwami/tests/romm-big.py <port file> <log file> <games per system> <achievements per game>
# It listens on 127.0.0.1, on a free port it writes to <port file> once it is ready, and appends each request to
# <log file> as "<method> <path>?<query>". Its games are made a page at a time, the same each run, in the shape of
# upstream's stand-in (ui/tests/fake-romm.py) plus what RomM 5.4.1's list sends beside them and banana-sync doesn't
# read: every provider's metadata, the RetroAchievements block with each achievement's 13 fields, screenshots (about
# 24 KB of JSON a game at 40 achievements). It answers:
#   GET /api/heartbeat                         version 5.4.1
#   GET /api/platforms                         nds, gba, gbc, snes, genesis (folder megadrive), psx, 3ds: ids 1 to 7
#   GET /api/roms                              platform_ids (or platform_id), limit, offset; items and total. Other
#                                              parameters are ignored, as RomM 5.4.1 does (with_provider_metadata too)
#   GET /api/roms/<id>/content/<name>          a game's one file: "rom <id>\n"
#   GET /api/roms/<file id>/files/content/<n>  a file of a game of several: "rom <file id>\n"
#   GET /assets/romm/resources/roms/...        a small PNG, or a JPEG for a .jpg
#   GET /api/saves, /api/saves/<id>/content    two saves: GBA's and DS's game 10
#   GET /api/states                            none
# Any "Bearer rmm_..." token is taken; anything else gets 403.
# Game i (from 1) of platform p: id p * 100000 + i, "Game <i:04> (USA).<ext>", one file. These are RomM's edge cases:
#   1 missing from RomM's disk             6 a path out of the folder (../Escape (USA))
#   2 null summary and cover, no metadatum 7 several files: a folder with a .cue, two .bin and a .txt
#   3 empty genres, publishers not a list, 8 no files[] (no hashes; the size from fs_size_bytes)
#     a release date in seconds, a .jpg   9 another platform's game in the page
#   4 a numeric name (1942)               11 a file whose file_size_bytes is null
#   5 a tab in the name
import hashlib
import http.server
import json
import os
import sys
import urllib.parse

PORT_FILE, LOG, N, ACH = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
PLATFORMS = [(1, "nds", "nds", "nds"), (2, "gba", "gba", "gba"), (3, "gbc", "gbc", "gbc"), (4, "snes", "snes", "sfc"),
             (5, "genesis", "megadrive", "md"), (6, "psx", "psx", "chd"), (7, "3ds", "3ds", "3ds")]
BY_ID = {p[0]: p for p in PLATFORMS}
PNG = bytes.fromhex("89504e470d0a1a0a0000000d4948445200000001000000010806000000"
                    "1f15c4890000000d4944415478da63f8ffff3f0005fe02fea7d6a4c00000000049454e44ae426082")
JPEG = bytes.fromhex("ffd8ffe000104a46494600010100000100010000ffd9")
TS = "?ts=2026-10-31 15:44:32.123456+00:00"
SAVES = {1: (200010, "Game 0010 (USA).srm", b"gba save 10\n"), 2: (100010, "Game 0010 (USA).sav", b"nds save 10\n")}


def content(fid):
    return f"rom {fid}\n".encode()


def file_entry(fid, rid, name, path, size_null=False):
    data = content(fid)
    return {"id": fid, "rom_id": rid, "file_name": name, "file_path": path, "file_size_bytes": None if size_null else len(data),
            "full_path": f"/romm/library/{path}/{name}", "crc_hash": None, "md5_hash": hashlib.md5(data).hexdigest(),
            "sha1_hash": hashlib.sha1(data).hexdigest(), "ra_hash": "", "category": None, "is_top_level": True}


def achievements(rid):
    return [{"ra_id": rid * 100 + k, "title": f"Achievement {k}", "description": f"Do the {k}th thing in game {rid} without losing a life.", "points": 5 + k % 20, "num_awarded": 1000 + k,
             "num_awarded_hardcore": 500 + k, "badge_id": str(300000 + k), "badge_url": f"https://media.retroachievements.org/Badge/{300000 + k}.png",
             "badge_url_lock": f"https://media.retroachievements.org/Badge/{300000 + k}_lock.png",
             "badge_path": f"/badges/{300000 + k}.png", "badge_path_lock": f"/badges/{300000 + k}_lock.png",
             "display_order": k, "type": "progression" if k % 3 else None} for k in range(ACH)]


def game(p, i):
    _, slug, fs, ext = BY_ID[p]
    rid = p * 100000 + i
    stem = f"Game {i:04} (USA)"
    name = f"Game {i:04}"
    multi = i == 7
    if i == 4:
        stem, name = "1942 (USA)", 1942
    if i == 5:
        name = f"Tab\tGame {i:04}"
    fs_name = stem if multi else f"{stem}.{ext}"
    if i == 6:
        fs_name = f"../Escape (USA).{ext}"
    if multi:
        names = [f"{stem}.cue", f"{stem} (Track 1).bin", f"{stem} (Track 2).bin", "readme.txt"]
        files = [file_entry(rid * 10 + k, rid, n, f"{fs}/{stem}") for k, n in enumerate(names)]
    elif i == 8:
        files = []
    else:
        files = [file_entry(rid * 10, rid, fs_name, fs, size_null=i == 11)]
    size = sum(len(content(f["id"])) for f in files) if files else len(content(rid))
    cover = f"/assets/romm/resources/roms/{p}/{rid}/cover/big.{'jpg' if i == 3 else 'png'}{TS}"
    meta = {"rom_id": rid, "genres": ["Action", "Puzzle"], "franchises": ["Banana"], "collections": [], "companies": ["Banana", "Peel"],
            "game_modes": ["Single player"], "age_ratings": ["E"], "first_release_date": 1262304000000 + i * 86400000,
            "publishers": ["Banana"], "developers": ["Peel"], "average_rating": 7.5}
    if i == 3:
        meta.update(genres=[], publishers="Banana", first_release_date=1262304000 + i * 86400)
    item = {
        "id": rid, "igdb_id": 9000 + i, "sgdb_id": None, "moby_id": None, "ss_id": 7000 + i, "ra_id": 5000 + i, "launchbox_id": 3000 + i,
        "hasheous_id": None, "tgdb_id": None, "flashpoint_id": None, "hltb_id": None,
        "platform_id": p % 7 + 1 if i == 9 else p, "platform_slug": slug, "platform_fs_slug": fs, "platform_custom_name": None,
        "platform_display_name": slug.upper(), "fs_name": fs_name, "fs_name_no_tags": stem.split(" (")[0], "fs_name_no_ext": stem,
        "fs_extension": "" if multi else ext, "fs_path": f"{fs}/roms", "fs_size_bytes": size, "name": name, "slug": f"game-{i}",
        "summary": None if i == 2 else f"About game {i} of {slug}. " + "A banana goes on an adventure through peel and pulp, "
                   "meeting friends and foes along the way, collecting coins and solving puzzles in twelve worlds. " * 2,
        "alternative_names": [f"Game {i} JP", f"Game {i} EU"], "youtube_video_id": None,
        "metadatum": meta, "path_cover_small": None if i == 2 else cover.replace("big", "small"),
        "path_cover_large": None if i == 2 else cover, "url_cover": f"https://images.igdb.com/igdb/image/upload/t_cover_big/{rid}.jpg",
        "has_manual": False, "path_manual": None, "url_manual": None, "is_identifying": False, "is_unidentified": False,
        "is_identified": True, "revision": "", "regions": ["USA"], "languages": ["En"], "tags": [], "crc_hash": None,
        "md5_hash": None, "sha1_hash": None, "has_simple_single_file": not multi and i != 8, "has_nested_single_file": False,
        "has_multiple_files": multi, "files": files, "full_path": f"{fs}/roms/{fs_name}", "created_at": "2026-10-01T10:00:00+00:00",
        "updated_at": "2026-10-31T15:44:32+00:00", "missing_from_fs": i == 1, "siblings": [], "rom_user": {"id": 1, "rom_id": rid,
        "is_main_sibling": False, "backlogged": False, "now_playing": False, "hidden": False, "rating": 0, "difficulty": 0,
        "completion": 0, "status": None, "last_played": None},
        "igdb_metadata": {"total_rating": "82.5", "aggregated_rating": "80.1", "first_release_date": 1262304000, "youtube_video_id": "x",
                          "genres": ["Platform", "Puzzle"], "franchises": ["Banana"], "alternative_names": [f"Game {i} JP"],
                          "collections": [], "companies": ["Banana", "Peel"], "game_modes": ["Single player"], "age_ratings": [
                              {"rating": "E", "category": "ESRB", "rating_cover_url": "https://example.org/esrb-e.png"}],
                          "platforms": [{"igdb_id": 24, "name": slug.upper()}], "expansions": [], "dlcs": [], "remasters": [],
                          "remakes": [], "expanded_games": [], "ports": [], "similar_games": [
                              {"id": 9000 + k, "name": f"Similar {k}", "slug": f"similar-{k}", "type": "game",
                               "cover_url": f"https://images.igdb.com/igdb/image/upload/t_cover_big/s{k}.jpg"} for k in range(4)]},
        "ss_metadata": {"ss_score": "16", "first_release_date": 1262304000, "alternative_names": [], "companies": ["Banana"],
                        "franchises": [], "game_modes": ["1 player"], "genres": ["Platform"], "box2d_url": "https://example.org/b.png",
                        "box3d_url": "https://example.org/b3.png", "marquee_url": "https://example.org/m.png",
                        "fanart_url": "https://example.org/f.png", "manual_url": None, "video_url": "https://example.org/v.mp4"},
        "launchbox_metadata": {"first_release_date": 1262304000, "max_players": 1, "release_type": "Released", "cooperative": False,
                               "youtube_video_id": "x", "community_rating": 4.1, "community_rating_count": 30, "wikipedia_url": "",
                               "esrb": "E - Everyone", "genres": ["Platform"], "companies": ["Banana"], "images": [
                                   {"url": f"https://images.launchbox-app.com/{rid}-{k}.png", "type": "Screenshot - Gameplay",
                                    "region": "North America"} for k in range(6)]},
        "hasheous_metadata": None, "moby_metadata": {}, "flashpoint_metadata": None, "hltb_metadata": None, "gamelist_metadata": None,
        "merged_ra_metadata": {"first_release_date": 1262304000000, "genres": ["Platform"], "companies": ["Banana"],
                               "achievements": achievements(rid)},
        "merged_screenshots": [f"/assets/romm/resources/roms/{p}/{rid}/screenshots/{k}.jpg" for k in range(6)],
    }
    if i == 2:
        del item["metadatum"]
    return item


class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json"):
        if not isinstance(body, bytes):
            body = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urllib.parse.urlsplit(self.path)
        with open(LOG, "a") as f:
            f.write(f"GET {u.path}?{u.query}\n")
        q = {k: v[-1] for k, v in urllib.parse.parse_qs(u.query).items()}
        p = urllib.parse.unquote(u.path)
        parts = p.split("/")
        if p == "/api/heartbeat":
            return self.send(200, {"SYSTEM": {"VERSION": "5.4.1", "GIT_BRANCH": None, "SHOW_SETUP_WIZARD": False}})
        if p.startswith("/assets/romm/resources/roms/"):
            return self.send(200, JPEG, "image/jpeg") if p.endswith(".jpg") else self.send(200, PNG, "image/png")
        auth = self.headers.get("Authorization", "")
        if not auth.startswith("Bearer rmm_"):
            return self.send(403, {"detail": "Forbidden"})
        if p == "/api/platforms":
            return self.send(200, [{"id": i, "slug": s, "fs_slug": fs, "name": s.upper(), "rom_count": N, "fs_size_bytes": 0,
                                    "custom_name": None, "category": None, "firmware": []} for i, s, fs, _ in PLATFORMS])
        if p == "/api/roms":
            pid = int(q.get("platform_ids", q.get("platform_id", 0)))
            lim, off = int(q.get("limit", 50)), int(q.get("offset", 0))
            if pid not in BY_ID:
                return self.send(200, {"items": [], "total": 0, "limit": lim, "offset": off})
            items = [game(pid, i) for i in range(off + 1, min(off + lim, N) + 1)]
            return self.send(200, {"items": items, "total": N, "limit": lim, "offset": off})
        if p.startswith("/api/roms/") and "/files/content/" in p:
            return self.send(200, content(int(parts[3])), "application/octet-stream")
        if p.startswith("/api/roms/") and "/content/" in p:
            return self.send(200, content(int(parts[3])), "application/octet-stream")
        if p == "/api/saves":
            pid = int(q.get("platform_id", 0))
            return self.send(200, [{"id": sid, "rom_id": rid, "file_name": fn, "file_name_no_ext": fn.rsplit(".", 1)[0],
                                    "file_extension": fn.rsplit(".", 1)[1], "file_size_bytes": len(data), "user_id": 1,
                                    "emulator": None, "slot": None, "missing_from_fs": False, "content_hash": hashlib.md5(data).hexdigest(),
                                    "updated_at": "2026-10-31T15:44:32.123456+00:00", "created_at": "2026-10-31T15:44:32.123456+00:00",
                                    "download_path": f"/api/saves/{sid}/content/{fn}", "screenshot": None}
                                   for sid, (rid, fn, data) in SAVES.items() if not pid or rid // 100000 == pid])
        if p.startswith("/api/saves/") and p.endswith("/content"):
            s = SAVES.get(int(parts[3]))
            return self.send(200, s[2], "application/octet-stream") if s else self.send(404, {"detail": "Save not found"})
        if p == "/api/states":
            return self.send(200, [])
        return self.send(404, {"detail": "Not Found"})


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True


srv = Server(("127.0.0.1", 0), H)
with open(PORT_FILE + ".new", "w") as f:
    f.write(f"{srv.server_address[1]}\n")
os.rename(PORT_FILE + ".new", PORT_FILE)
srv.serve_forever()
