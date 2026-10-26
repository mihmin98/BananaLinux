// The RomM helper (kiwami), /usr/libexec/kiwami/romm: everything of the fork's RomM client that talks to the server,
// for Settings' RomM pages and the wrapper around the games (romm-game). One verb a run:
//   romm check     one line to stdout: ok <version>, newer <version>, old <version>, unpaired, revoked or down;
//                  exits 0 for ok and newer
//   romm pair      pairs with accounts.conf's romm_url by RomM's device flow: prints (and puts in the status file)
//                  "pairing <code>\t<the page to approve it on>", then goes on in the background, asking RomM every
//                  few seconds until the user approves it (the token and the device written to accounts.conf, the
//                  status "paired <version>", the last sync's result dropped), denies it or lets it expire
//                  ("pair-failed denied|expired"). A second pair stops the first. Failing at once, it prints the
//                  pair-failed line and exits 1
//   romm logout    asks RomM to delete the device (which takes its token away; skipped with no server), then takes the
//                  token and the device out of accounts.conf
//   romm pull <system> <rom>
//                  before a game: RomM's autosave of it put in place when it is newer than the card's (the old one
//                  kept as .bak), within 3 s in all; a conflict (both changed) is noted and nothing is overwritten.
//                  Whatever fails, it exits 0 and the game starts with the card's save
//   romm push <system> <rom> <start> <end>
//                  after a game, in the background: its save sent to RomM with the play's times (seconds since 1970),
//                  and whatever is pending with it; kept pending when RomM can't be reached
//   romm pending <system> <rom> <start> <end>
//                  the push and the play kept for later, with no network (after a shutdown)
//   romm sync      Sync now, in the background: what is pending first, then every game of the synced systems on both
//                  cards, each with its save or RomM's, negotiated 500 at a time. The status file says
//                  "running <done> <total>\t<system>\t<game>", then "done sync <sent> <fetched> <conflicts> <skipped>"
//                  (also kept in <card>/banana/romm/last) or "error sync down|revoked|old|unpaired". With a sync or a
//                  push already running it prints busy and exits 3
//   romm resolve <n> local|server <rom>
//                  the n-th conflict (its line in <card>/banana/romm/conflicts, which has to be still <rom>'s): this
//                  card's save sent over RomM's (local), or RomM's taken, the card's kept as .bak (server). Prints
//                  "resolved <n> local|server", or "resolve-failed <why>" and exits 1 with the line left; busy (3) when
//                  the saves are still locked after 10 s
//   romm platforms one line each of RomM's platforms that are Banana's systems' (the nine above's):
//                  "<id>\t<slug>\t<system>\t<games>\t<name>"
//   romm roms <platform id> <offset> [search]
//                  "total <n>", then the platform's games from offset, 50 at most, by name, each
//                  "<id>\t<size>\t<on a card: 0|1>\t<fs_name>\t<name>"; on a card is in <card>/banana/romm/games, or a
//                  file or folder of that name in the system's folder on either card. Each of these two says
//                  "error <verb> down|revoked|old|unpaired" and exits 1 when it can't list
//   romm download <rom id>
//                  in the background: the game onto romm_target's card (the main card when the second isn't in, which
//                  the status says), free space checked first, resumed from a download cut short (download.h). The
//                  status file says "running <bytes> <total>\t<system>\t<name>\t<main card: the second card isn't
//                  in|>", then "done download <id> <path>" or "error download <why>" (down, cut, revoked, old,
//                  unpaired, notfound, system, name, there <path>, space <need> <free>, card, unpack <zip>, size)
//   romm bios      in the background: RomM's firmware of those platforms into <card>/bios, a file there with the same
//                  MD5 skipped, one with another kept as .bak. "running <done> <total>\tbios\t<file>", then
//                  "done bios <fetched> <skipped> <failed>" or "error bios down|revoked|old|unpaired"
//                  With a download or bios already running, each prints busy and exits 3
// The saves are changed under one lock (<run>/romm-saves.lock): the pull waits for it within its 3 s, the push up to 60 s.
// The status file is <run>/romm.status, one line; the log <card>/banana/logs/romm.log (http.h), which the token never
// reaches: the helper reads it from accounts.conf, never from its command line. The paths: rommcard.h.
// KIWAMI_ROMM_SYNC=1 keeps it in the foreground (the tests); KIWAMI_MODEL moves /proc/device-tree/model.
#include "api.h"
#include "download.h"
#include "games.h"
#include "http.h"
#include "saves.h"
#include "../conf.h"
#include "../rommcard.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace banana::kiwami;
using namespace banana::kiwami::romm;

namespace
{
	Card card;

	std::string firstLine(const std::string& path)
	{
		std::ifstream f(path);
		std::string ln;
		std::getline(f, ln);
		std::string o;
		for (char c : ln) if (c != '\0' && c != '\r') o += c;
		return o;
	}

	void status(const std::string& line)
	{
		std::string error;
		makeDir(card.run);
		if (!replaceFile(card.status, line + "\n", error)) log("%s", error.c_str());
	}

	// a line to stdout for the caller (Settings' job, the tests), and into the log
	void say(const std::string& line)
	{
		printf("%s\n", line.c_str());
		fflush(stdout);
		log("%s: %s", verb.c_str(), line.c_str());
	}

	bool sync() { const char* e = getenv("KIWAMI_ROMM_SYNC"); return e && strcmp(e, "1") == 0; }

	// into the background, out of the caller's session, its pipes let go: true in the child, which goes on; the parent
	// exits. With KIWAMI_ROMM_SYNC=1, nothing: the work goes on in the foreground
	bool detach()
	{
		if (sync()) return true;
		fflush(stdout);
		pid_t p = fork();
		if (p < 0) return true;
		if (p > 0) _exit(0);
		setsid();
		int fd = open("/dev/null", O_RDWR);
		if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
		return true;
	}

	std::string deviceName()
	{
		const char* e = getenv("KIWAMI_MODEL");
		std::string model = firstLine(e && *e ? e : "/proc/device-tree/model");
		return model.empty() ? "BananaOS" : "BananaOS on " + model;
	}

	Server server(const Account& a, long connectMs, long totalMs)
	{
		Server s;
		s.url = a.url;
		s.token = a.token;
		s.connectMs = connectMs;
		s.totalMs = totalMs;
		return s;
	}

	// the server's version, checked: false (and the reason in why: down, or old <v>) when it can't be synced with
	bool versionChecked(const Server& s, std::string& version, std::string& why)
	{
		Heartbeat h = heartbeat(s);
		if (!h.ok) { why = "down"; log("%s: no server (%s)", verb.c_str(), h.error.c_str()); return false; }
		version = h.version;
		Version v = versionOk(h.version);
		if (v == OLD) { why = "old " + h.version; log("%s: RomM %s is older than 5.4.0, refused", verb.c_str(), h.version.c_str()); return false; }
		if (v == NEWER) log("%s: RomM %s is newer than 5.4, which this client was written for", verb.c_str(), h.version.c_str());
		return true;
	}

	// ---- pairing ---------------------------------------------------------------------------------------------------
	std::string pairPid() { return card.run + "/romm-pair.pid"; }

	// a pairing still waiting, stopped (its pid file names it, and it is this helper's pair)
	void stopPairing()
	{
		pid_t p = (pid_t)atol(firstLine(pairPid()).c_str());
		if (p <= 0 || p == getpid()) return;
		std::ifstream f("/proc/" + std::to_string(p) + "/cmdline", std::ios::binary);
		std::stringstream ss;
		ss << f.rdbuf();
		std::string cmd = ss.str();
		size_t a1 = cmd.find('\0');
		if (a1 == std::string::npos || cmd.compare(a1 + 1, 5, std::string("pair\0", 5)) != 0) return;
		std::string arg0 = cmd.substr(0, a1);
		if (arg0.size() < 4 || arg0.compare(arg0.size() - 4, 4, "romm") != 0) return;
		if (kill(p, SIGTERM) != 0) return;
		for (int i = 0; i < 50 && kill(p, 0) == 0; i++) usleep(20000);
		log("%s: the pairing waiting before (%d) stopped", verb.c_str(), (int)p);
	}

	int pair()
	{
		Account a = account(card);
		if (a.url.empty()) { status("pair-failed noserver"); say("pair-failed noserver"); return 1; }
		stopPairing();
		Server s = server(a, 3000, 10000);
		s.token.clear();
		std::string version, why;
		if (!versionChecked(s, version, why)) { status("pair-failed " + why); say("pair-failed " + why); return 1; }
		std::string id = deviceIdentifier(card);
		if (id.empty()) { status("pair-failed card"); say("pair-failed card"); return 1; }
		DeviceInit d = deviceInit(s, deviceName(), id, firstLine(card.etc + "/version"));
		if (!d.ok)
		{
			log("pair: refused (%s)", d.error.c_str());
			status("pair-failed down");
			say("pair-failed down");
			return 1;
		}
		std::string line = "pairing " + d.userCode + "\t" + a.url + d.path;
		status(line);
		say(line);
		detach();
		{
			std::string error;
			if (!replaceFile(pairPid(), std::to_string(getpid()) + "\n", error)) log("pair: %s", error.c_str());
		}
		time_t deadline = time(nullptr) + d.expiresIn;
		int interval = d.interval < 1 ? 1 : d.interval;
		std::string result;
		while (result.empty())
		{
			sleep(interval);
			if (time(nullptr) > deadline) { result = "pair-failed expired"; break; }
			DeviceToken t = deviceToken(s, d.deviceCode);
			switch (t.state)
			{
			case DeviceToken::GRANTED:
			{
				std::string error;
				if (paired(card, t.token, t.deviceId, error))
				{
					result = "paired " + version;
					// the last sync's result was the old pairing's (a revoked one would keep Sync now refusing)
					if (unlink((card.dir + "/last").c_str()) == 0) log("pair: the last sync's result dropped");
				}
				else { log("pair: %s", error.c_str()); result = "pair-failed card"; }
				break;
			}
			case DeviceToken::SLOW_DOWN: interval += 5; break;
			case DeviceToken::DENIED: result = "pair-failed denied"; break;
			case DeviceToken::EXPIRED: result = "pair-failed expired"; break;
			case DeviceToken::PENDING: break;
			case DeviceToken::FAILED: break;	// (the network back in a moment: asked again until it expires)
			}
		}
		status(result);
		log("pair: %s", result.c_str());
		if (atol(firstLine(pairPid()).c_str()) == getpid()) unlink(pairPid().c_str());
		return result.compare(0, 7, "paired ") == 0 ? 0 : 1;
	}

	// ---- the saves ------------------------------------------------------------------------------------------------
	long long nowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	// the pull's budget: every request's timeouts cut to what is left of it; false (no request) with under 200 ms left.
	// With no deadline, the server's own timeouts
	long long deadline = 0;
	bool ready(Server& s)
	{
		if (!deadline) return true;
		long long left = deadline - nowMs();
		if (left < 200) { log("%s: out of time", verb.c_str()); return false; }
		s.connectMs = left < 1500 ? (long)left : 1500;
		s.totalMs = (long)left;
		return true;
	}

	// RomM refused the token: noted, so Settings' rows say to pair again
	bool revoked = false;
	void noteRevoked()
	{
		if (revoked) return;
		revoked = true;
		log("%s: RomM refused the token (401): pair again", verb.c_str());
		Last l = last(card);
		l.when = time(nullptr);
		l.state = "revoked";
		std::string error;
		if (!setLast(card, l, error)) log("%s: %s", verb.c_str(), error.c_str());
	}

	// the saves lock, waited for up to waitMs; false when another helper still holds it
	bool lockSaves(long long waitMs)
	{
		makeDir(card.run);
		int fd = open((card.run + "/romm-saves.lock").c_str(), O_RDWR | O_CREAT, 0644);
		if (fd < 0) return true;	// (no /run: nothing to keep apart)
		long long until = nowMs() + waitMs;
		bool told = false;
		while (flock(fd, LOCK_EX | LOCK_NB) != 0)
		{
			if (nowMs() >= until) { close(fd); return false; }
			if (!told) { log("%s: waiting for another sync to end", verb.c_str()); told = true; }
			usleep(50000);
		}
		return true;	// (held until the helper exits)
	}

	bool pairedHere(const Account& a)
	{
		if (!a.url.empty() && !a.token.empty() && !a.deviceId.empty()) return true;
		log("%s: not paired, nothing to sync", verb.c_str());
		return false;
	}

	// a game being synced: its save on the card, read, and its RomM id
	struct Game
	{
		std::string system, rom;
		Save save;
		long id = 0;
		bool hasSave = false;
		std::string bytes, hash;
		long long mtimeMs = 0;
		std::string name() const { return save.stem.empty() ? rom : save.stem; }
		ClientSave client() const
		{
			ClientSave c;
			c.romId = id;
			c.fileName = save.path.substr(save.path.rfind('/') + 1);
			c.emulator = save.emulator;
			c.hash = hash;
			c.mtimeMs = mtimeMs;
			c.size = (long long)bytes.size();
			return c;
		}
	};

	// the game's save found and read, and its id: false (logged) for a game that isn't synced or isn't on RomM
	bool prepare(Game& g, const std::vector<System>& sys, Games& lookup)
	{
		const System* s = findSystem(sys, g.system);
		if (!s || !synced(g.system)) { log("%s: %s: %s isn't a system whose saves are synced", verb.c_str(), g.rom.c_str(), g.system.c_str()); return false; }
		std::string why;
		if (!saveOf(card, *s, g.rom, g.save, why)) { log("%s: %s: skipped, %s", verb.c_str(), g.rom.c_str(), why.c_str()); return false; }
		g.id = lookup.idOf(*s, g.rom);
		if (!g.id) return false;
		// (nds-saves only when there is a save for it to look at: Sync now goes through every DS game)
		std::string beside = g.rom.substr(0, g.rom.rfind('.')) + ".dsv";
		if (g.save.ds && (!access(g.save.path.c_str(), F_OK) || !access(g.save.dsv.c_str(), F_OK) || !access(beside.c_str(), F_OK)))
			ndsSaves(card, g.rom);
		g.hasSave = readSave(g.save, g.bytes, g.mtimeMs);
		if (g.hasSave) g.hash = md5Hex(g.bytes);
		return true;
	}

	struct Tally { int sent = 0, fetched = 0, conflicts = 0, done = 0, failed = 0; };

	void noteConflictOf(const Game& g, long saveId, const std::string& serverTime)
	{
		Conflict k;
		k.system = g.system;
		k.rom = g.rom;
		k.romId = g.id;
		k.saveId = saveId;
		k.serverTime = serverTime;
		k.noted = time(nullptr);
		std::string error;
		if (!noteConflict(card, k, error)) log("%s: %s", verb.c_str(), error.c_str());
	}

	// the game in step with RomM: no conflict, and the save's hash (none: "") kept as at this sync
	void cleared(const Game& g)
	{
		std::string error;
		if (!clearConflict(card, g.system, g.rom, error)) log("%s: %s", verb.c_str(), error.c_str());
		if (!setSynced(card, g.id, g.hasSave ? g.hash : "", error)) log("%s: %s", verb.c_str(), error.c_str());
	}

	// RomM's save in place of the card's, and RomM told what the card now has
	bool fetchSave(Server& s, const Account& a, long session, Game& g, const Operation& o, Tally& t)
	{
		std::string bytes, error;
		if (!ready(s)) return false;
		long st = downloadSave(s, o.saveId, a.deviceId, session, bytes);
		if (st == 401) noteRevoked();
		if (st != 200) return false;
		if (!o.serverHash.empty() && md5Hex(bytes) != o.serverHash)
		{
			log("%s: %s: RomM's save didn't arrive whole (its hash differs), left alone", verb.c_str(), g.name().c_str());
			return false;
		}
		if (!replaceSave(card, g.save, bytes, error)) { log("%s: %s", verb.c_str(), error.c_str()); return false; }
		// (the DS's as nds-saves left it: raw, which the next negotiation sends)
		g.hasSave = readSave(g.save, g.bytes, g.mtimeMs);
		g.hash = g.hasSave ? md5Hex(g.bytes) : "";
		if (ready(s) && confirmDownload(s, o.saveId, a.deviceId, g.hash) == 401) noteRevoked();
		t.fetched++;
		cleared(g);
		log("%s: %s: RomM's save taken (from %s)", verb.c_str(), g.name().c_str(), o.emulator.empty() ? "an unnamed emulator" : o.emulator.c_str());
		return true;
	}

	// one of RomM's operations carried out on its game: false when it failed (worth trying again)
	bool apply(Server& s, const Account& a, long session, Game& g, const Operation& o, Tally& t)
	{
		std::string error;
		if (o.action == "upload")
		{
			if (!g.hasSave) return true;
			if (!ready(s)) return false;
			Upload u = uploadSave(s, g.client(), a.deviceId, session, false, g.bytes);
			if (u.ok) { t.sent++; cleared(g); log("%s: %s: sent (%s)", verb.c_str(), g.name().c_str(), g.save.emulator.c_str()); return true; }
			if (u.conflict)
			{
				noteConflictOf(g, 0, "");
				t.conflicts++;
				log("%s: %s: RomM has a newer save since this handheld's last sync (409): a conflict, kept for Settings", verb.c_str(), g.name().c_str());
				return true;
			}
			if (u.status == 401) noteRevoked();
			return false;
		}
		if (o.action == "download")
		{
			// the card's save changed since the last sync too: RomM didn't see it (a newer save from another device is a
			// row of its own, which this handheld has no sync of), so a conflict, nothing overwritten
			std::string was = syncedHash(card, g.id);
			if (g.hasSave && !was.empty() && was != g.hash)
			{
				noteConflictOf(g, o.saveId, o.serverTime);
				t.conflicts++;
				log("%s: %s: changed here since the last sync, and RomM has a newer one: a conflict, nothing overwritten", verb.c_str(),
					g.name().c_str());
				return true;
			}
			return fetchSave(s, a, session, g, o, t);
		}
		if (o.action == "conflict")
		{
			noteConflictOf(g, o.saveId, o.serverTime);
			t.conflicts++;
			log("%s: %s: changed here and on RomM since the last sync: a conflict, nothing overwritten", verb.c_str(), g.name().c_str());
			return true;
		}
		if (o.action == "delete")
		{
			if (!dropSave(g.save, error)) { log("%s: %s", verb.c_str(), error.c_str()); return false; }
			g.hasSave = false;
			cleared(g);
			return true;
		}
		cleared(g);		// (no_op: the same on both sides)
		return true;
	}

	std::vector<System> systemsOnce;
	const std::vector<System>& allSystems()
	{
		if (systemsOnce.empty()) systemsOnce = systems(card);
		return systemsOnce;
	}

	int pull(const std::string& system, const std::string& rom)
	{
		deadline = nowMs() + 3000;
		Account a = account(card);
		if (!pairedHere(a)) return 0;
		Server s = server(a, 1500, 3000);
		if (!lockSaves(deadline - nowMs() - 200)) { log("pull: another sync holds the saves, the game starts with the card's"); return 0; }
		std::string version, why;
		if (!ready(s) || !versionChecked(s, version, why)) return 0;
		Games lookup(card, s, false, ready);
		Game g;
		g.system = system;
		g.rom = rom;
		if (!prepare(g, allSystems(), lookup)) { if (lookup.status == 401) noteRevoked(); return 0; }
		std::vector<ClientSave> saves;
		if (g.hasSave) saves.push_back(g.client());
		if (!ready(s)) return 0;
		Negotiation n = negotiate(s, a.deviceId, saves, { g.id });
		if (n.status == 401) noteRevoked();
		if (!n.ok) { log("pull: %s: no negotiation (%s)", g.name().c_str(), n.error.c_str()); return 0; }
		Tally t;
		bool any = false;
		for (auto& o : n.ops)
		{
			if (o.romId != g.id || o.slot != "autosave") continue;
			any = true;
			if (o.action == "upload") { log("pull: %s: the card's is newer, it goes up after the game", g.name().c_str()); continue; }
			if (o.action == "no_op") log("pull: %s: the same as RomM's", g.name().c_str());
			(apply(s, a, n.session, g, o, t) ? t.done : t.failed)++;
		}
		if (!any) log("pull: %s: no save on either side", g.name().c_str());
		if (ready(s) && complete(s, n.session, t.done, t.failed, {}) == 401) noteRevoked();
		return 0;
	}

	int push(const std::string& system, const std::string& rom, long long start, long long end)
	{
		Pending mine;
		mine.pushes.push_back({ system, rom });
		mine.plays.push_back({ system, rom, start, end });
		std::string error;
		auto keep = [&](const char* why) {
			log("push: %s: %s, kept for the next sync", rom.c_str(), why);
			if (!addPending(card, mine, error)) log("push: %s", error.c_str());
			return 0;
		};
		detach();
		if (!lockSaves(60000)) return keep("another sync still ran after 60 s");
		Account a = account(card);
		if (!pairedHere(a)) return 0;
		Server s = server(a, 5000, 30000);
		std::string version, why;
		if (!versionChecked(s, version, why)) return keep(why == "down" ? "no server" : "RomM too old");
		// this game first, then the pending ones, each once (a game on both cards is one save: one RomM id)
		Pending carried = pending(card);
		std::vector<Game> games;
		std::vector<Pending::Push> want = mine.pushes;
		want.insert(want.end(), carried.pushes.begin(), carried.pushes.end());
		Games lookup(card, s, true);
		for (auto& p : want)
		{
			Game g;
			g.system = p.system;
			g.rom = p.rom;
			bool dup = false;
			for (auto& o : games) dup |= o.system == g.system && o.rom == g.rom;
			if (dup || !prepare(g, allSystems(), lookup)) { if (lookup.down) break; continue; }
			for (auto& o : games) dup |= o.id == g.id;
			if (!dup) games.push_back(g);
		}
		if (lookup.status == 401) noteRevoked();
		if (lookup.down) return keep(revoked ? "RomM refused the token" : "no list of RomM's games");
		std::vector<ClientSave> saves;
		std::vector<long> ids;
		for (auto& g : games) { ids.push_back(g.id); if (g.hasSave) saves.push_back(g.client()); }
		Tally t;
		std::vector<bool> failed(games.size(), false);
		long session = 0;
		bool negotiated = games.empty();
		if (!games.empty())
		{
			Negotiation n = negotiate(s, a.deviceId, saves, ids);
			if (n.status == 401) noteRevoked();
			if (!n.ok) return keep(revoked ? "RomM refused the token" : "no negotiation");
			negotiated = true;
			session = n.session;
			for (auto& o : n.ops)
			{
				if (o.slot != "autosave") continue;
				for (size_t i = 0; i < games.size(); i++)
					if (games[i].id == o.romId)
					{
						if (o.action == "no_op") log("push: %s: the same as RomM's", games[i].name().c_str());
						if (apply(s, a, session, games[i], o, t)) t.done++;
						else { t.failed++; failed[i] = true; }
					}
			}
		}
		// the plays: this one and the pending ones, each with its game's id (one RomM doesn't have isn't sent)
		std::vector<Pending::Play> playsAll = mine.plays;
		playsAll.insert(playsAll.end(), carried.plays.begin(), carried.plays.end());
		std::vector<PlaySession> plays;
		for (auto& p : playsAll)
		{
			const System* sy = findSystem(allSystems(), p.system);
			long id = sy ? lookup.idOf(*sy, p.rom) : 0;
			if (id) plays.push_back({ id, p.start, p.end });
		}
		bool playsSent = true;
		if (negotiated && session)
		{
			long st = complete(s, session, t.done, t.failed, plays);
			if (st == 401) noteRevoked();
			playsSent = st == 200;
		}
		else if (!plays.empty())
		{
			long st = playSessions(s, a.deviceId, plays);
			if (st == 401) noteRevoked();
			playsSent = st == 201;
		}
		// the pending file: what was carried taken out, what failed put back
		Pending back;
		for (size_t i = 0; i < games.size(); i++) if (failed[i]) back.pushes.push_back({ games[i].system, games[i].rom });
		if (!playsSent) back.plays = playsAll;
		if (!removePending(card, carried, error)) log("push: %s", error.c_str());
		if (!back.empty() && !addPending(card, back, error)) log("push: %s", error.c_str());
		log("push: done: %d sent, %d taken from RomM, %d conflicts, %d failed, %zu plays %s", t.sent, t.fetched, t.conflicts, t.failed,
			plays.size(), playsSent ? "sent" : "kept");
		return 0;
	}

	int keepPending(const std::string& system, const std::string& rom, long long start, long long end)
	{
		Pending p;
		p.pushes.push_back({ system, rom });
		p.plays.push_back({ system, rom, start, end });
		std::string error;
		if (!addPending(card, p, error)) { log("pending: %s", error.c_str()); return 1; }
		log("pending: %s: the push and the play kept for the next sync", rom.c_str());
		return 0;
	}

	// ---- Sync now -------------------------------------------------------------------------------------------------
	std::string lower(std::string s)
	{
		for (auto& c : s) c = (char)tolower((unsigned char)c);
		return s;
	}

	// a system's folder's games as the library lists them (library.cpp's listRoms): by ext, without case (an ext can
	// have dots of its own), two folders deep unless flat, art's and ES's folders and dot files left out; in name order
	void listRoms(const std::string& dir, const std::vector<std::string>& exts, int depth, int maxDepth, std::vector<std::string>& out)
	{
		DIR* d = opendir(dir.c_str());
		if (!d) return;
		std::vector<std::pair<std::string, bool>> entries;
		while (dirent* e = readdir(d))
		{
			if (e->d_name[0] == '.') continue;
			bool isDir = e->d_type == DT_DIR;
			if (e->d_type == DT_UNKNOWN || e->d_type == DT_LNK)
			{
				struct stat st;
				if (stat((dir + "/" + e->d_name).c_str(), &st)) continue;
				isDir = S_ISDIR(st.st_mode);
			}
			entries.push_back({ e->d_name, isDir });
		}
		closedir(d);
		std::sort(entries.begin(), entries.end());
		for (auto& [name, isDir] : entries)
		{
			std::string ln = lower(name);
			if (isDir)
			{
				if (depth < maxDepth && ln != "media" && ln != "images" && ln != "videos" && ln != "manuals" && ln != "downloaded_images")
					listRoms(dir + "/" + name, exts, depth + 1, maxDepth, out);
				continue;
			}
			for (const std::string& x : exts)
				if (ln.size() > x.size() + 1 && ln.compare(ln.size() - x.size() - 1, std::string::npos, "." + x) == 0) { out.push_back(dir + "/" + name); break; }
		}
	}

	// the cards' roms folders: this card's, and the second card's while it's in (bananad's sd2 script names it)
	std::vector<std::string> romRoots()
	{
		std::vector<std::string> r = { card.card + "/roms" };
		std::string sd2 = sd2Roms(card);
		if (!sd2.empty() && sd2 != r[0]) r.push_back(sd2);
		return r;
	}

	// the sync's end when it can't go on: why is down, revoked, old or unpaired
	int syncStopped(const std::string& why)
	{
		if (why == "down")
		{
			Last l = last(card);
			l.when = time(nullptr);
			l.state = "down";
			std::string error;
			if (!setLast(card, l, error)) log("sync: %s", error.c_str());
		}
		status("error sync " + why);
		log("sync: stopped: %s", why.c_str());
		return 1;
	}

	int syncAll()
	{
		Account a = account(card);
		if (!pairedHere(a)) { status("error sync unpaired"); say("error sync unpaired"); return 1; }
		if (!lockSaves(0)) { say("busy"); return 3; }		// (taken before going into the background: the child keeps it)
		status("running 0 0\t\t");
		detach();
		Server s = server(a, 5000, 30000);
		std::string version, why;
		if (!versionChecked(s, version, why)) return syncStopped(why == "down" ? "down" : "old");

		// the games: the pending ones first, then each synced system's on both cards, one per save (system and stem)
		Pending carried = pending(card);
		std::vector<Game> listed;
		std::set<std::string> keys;
		auto add = [&](const std::string& system, const std::string& rom) {
			if (!keys.insert(system + "\t" + stemOf(rom)).second) return;
			Game g;
			g.system = system;
			g.rom = rom;
			listed.push_back(g);
		};
		for (auto& p : carried.pushes) add(p.system, p.rom);
		std::vector<std::string> roots = romRoots();
		for (auto& sy : allSystems())
		{
			if (!synced(sy.id)) continue;
			for (size_t r = 0; r < roots.size(); r++)
			{
				bool absolute = !sy.folder.empty() && sy.folder[0] == '/';
				if (r > 0 && absolute) continue;	// (an absolute folder: one place)
				std::vector<std::string> roms;
				listRoms(absolute ? sy.folder : roots[r] + "/" + sy.folder, sy.ext, 0, sy.flat ? 0 : 2, roms);
				for (auto& rom : roms) add(sy.id, rom);
			}
		}

		// each one's save and RomM id; one RomM game is one save (a game on both cards, or a disc's .m3u and its .cue):
		// the one with a save kept, the newest save when both have one
		int total = (int)listed.size(), done = 0, skipped = 0;
		auto progress = [&](const Game& g) {
			status("running " + std::to_string(done) + " " + std::to_string(total) + "\t" + g.system + "\t" + g.name());
		};
		Games lookup(card, s, true);
		lookup.batch = true;
		std::vector<Game> games;
		for (auto& g : listed)
		{
			g.save.stem = stemOf(g.rom);
			progress(g);
			if (!prepare(g, allSystems(), lookup))
			{
				if (lookup.down) break;
				skipped++;
				done++;
				continue;
			}
			auto same = std::find_if(games.begin(), games.end(), [&](const Game& o) { return o.id == g.id; });
			if (same == games.end()) games.push_back(g);
			else
			{
				if (g.hasSave && (!same->hasSave || g.mtimeMs > same->mtimeMs)) *same = g;
				done++;
			}
		}
		if (lookup.status == 401) noteRevoked();
		if (lookup.down) return syncStopped(revoked ? "revoked" : "down");
		lookup.flush();

		// the pending plays, sent with the first negotiation's session
		std::vector<PlaySession> plays;
		for (auto& p : carried.plays)
		{
			const System* sy = findSystem(allSystems(), p.system);
			long id = sy ? lookup.idOf(*sy, p.rom) : 0;
			if (id) plays.push_back({ id, p.start, p.end });
		}
		bool playsSent = plays.empty();

		Tally t;
		std::vector<bool> failed(games.size(), false);
		for (size_t from = 0; from < games.size(); from += 500)
		{
			size_t to = std::min(games.size(), from + 500);
			std::vector<ClientSave> saves;
			std::vector<long> ids;
			for (size_t i = from; i < to; i++) { ids.push_back(games[i].id); if (games[i].hasSave) saves.push_back(games[i].client()); }
			Negotiation n = negotiate(s, a.deviceId, saves, ids);
			if (n.status == 401) noteRevoked();
			if (!n.ok) return syncStopped(revoked ? "revoked" : "down");
			int batchDone = 0, batchFailed = 0;
			for (size_t i = from; i < to; i++)
			{
				progress(games[i]);
				for (auto& o : n.ops)
				{
					if (o.romId != games[i].id || o.slot != "autosave") continue;
					if (apply(s, a, n.session, games[i], o, t)) batchDone++;
					else { batchFailed++; failed[i] = true; }
				}
				done++;
			}
			t.done += batchDone;
			t.failed += batchFailed;
			long st = complete(s, n.session, batchDone, batchFailed, from == 0 ? plays : std::vector<PlaySession>());
			if (st == 401) noteRevoked();
			if (from == 0) playsSent = playsSent || st == 200;
			if (revoked) return syncStopped("revoked");
		}
		if (!playsSent && games.empty())
		{
			long st = playSessions(s, a.deviceId, plays);
			if (st == 401) noteRevoked();
			playsSent = st == 201;
		}

		// the pending file: what was carried taken out, what failed put back (the next push after a game tries it again)
		std::string error;
		Pending gone, back;
		gone.pushes = carried.pushes;
		if (playsSent) gone.plays = carried.plays;
		for (size_t i = 0; i < games.size(); i++) if (failed[i]) back.pushes.push_back({ games[i].system, games[i].rom });
		if (!removePending(card, gone, error)) log("sync: %s", error.c_str());
		if (!back.empty() && !addPending(card, back, error)) log("sync: %s", error.c_str());

		Last l;
		l.when = time(nullptr);
		l.conflicts = (int)conflicts(card).size();
		l.state = l.conflicts ? "conflicts" : "synced";
		l.sent = t.sent;
		l.fetched = t.fetched;
		l.skipped = skipped;
		if (!setLast(card, l, error)) log("sync: %s", error.c_str());
		status("done sync " + std::to_string(l.sent) + " " + std::to_string(l.fetched) + " " + std::to_string(l.conflicts) + " "
			+ std::to_string(l.skipped));
		log("sync: done: %d games, %d sent, %d taken from RomM, %d conflicts, %d not on RomM, %d failed, %zu plays %s", total, l.sent,
			l.fetched, l.conflicts, l.skipped, t.failed, plays.size(), playsSent ? "sent" : "kept");
		return 0;
	}

	// a conflict settled: last's count (and its state, when it was a sync's result) in step with the list
	void recount()
	{
		Last l = last(card);
		if (!l.when) return;
		l.conflicts = (int)conflicts(card).size();
		if (l.state == "conflicts" || l.state == "synced") l.state = l.conflicts ? "conflicts" : "synced";
		std::string error;
		if (!setLast(card, l, error)) log("resolve: %s", error.c_str());
	}

	int resolve(int n, const std::string& side, const std::string& rom)
	{
		auto refused = [](const std::string& why) { say("resolve-failed " + why); return 1; };
		Account a = account(card);
		if (!pairedHere(a)) return refused("unpaired");
		if (!lockSaves(10000)) { say("busy"); return 3; }
		std::vector<Conflict> ks = conflicts(card);
		if (n < 1 || n > (int)ks.size() || ks[n - 1].rom != rom)
		{
			log("resolve: the conflicts changed: line %d isn't %s now", n, rom.c_str());
			return refused("changed");
		}
		Conflict k = ks[n - 1];
		Server s = server(a, 5000, 30000);
		std::string version, why;
		if (!versionChecked(s, version, why)) return refused(why == "down" ? "down" : "old");
		Game g;
		g.system = k.system;
		g.rom = k.rom;
		g.id = k.romId;
		const System* sy = findSystem(allSystems(), k.system);
		if (!sy || !saveOf(card, *sy, k.rom, g.save, why)) { log("resolve: %s: %s", k.rom.c_str(), sy ? why.c_str() : "no such system"); return refused("card"); }
		if (g.save.ds) ndsSaves(card, g.rom);
		g.hasSave = readSave(g.save, g.bytes, g.mtimeMs);
		if (g.hasSave) g.hash = md5Hex(g.bytes);
		Tally t;
		if (side == "local")
		{
			if (!g.hasSave) { log("resolve: %s: no save on the card to keep", g.name().c_str()); return refused("nosave"); }
			Upload u = uploadSave(s, g.client(), a.deviceId, 0, true, g.bytes);
			if (u.status == 401) { noteRevoked(); return refused("revoked"); }
			if (!u.ok) return refused("down");
			cleared(g);
			log("resolve: %s: this card's save sent over RomM's (%s)", g.name().c_str(), g.save.emulator.c_str());
		}
		else
		{
			std::vector<ClientSave> saves;
			if (g.hasSave) saves.push_back(g.client());
			Negotiation ng = negotiate(s, a.deviceId, saves, { g.id });
			if (ng.status == 401) { noteRevoked(); return refused("revoked"); }
			if (!ng.ok) return refused("down");
			const Operation* op = nullptr;
			for (auto& o : ng.ops) if (o.romId == g.id && o.slot == "autosave") op = &o;
			bool ok = false;
			std::string error;
			if (!op) log("resolve: %s: RomM has no save of it now", g.name().c_str());
			else if (op->action == "no_op") { cleared(g); ok = true; }
			else if (op->action == "delete")
			{
				ok = dropSave(g.save, error);
				if (ok) { g.hasSave = false; cleared(g); }
				else log("resolve: %s", error.c_str());
			}
			else if (op->saveId) ok = fetchSave(s, a, ng.session, g, *op, t);	// (download, conflict, or upload over RomM's)
			else log("resolve: %s: RomM has no save of it now", g.name().c_str());
			if (complete(s, ng.session, ok, !ok, {}) == 401) noteRevoked();
			if (!ok) return refused(revoked ? "revoked" : op && (op->saveId || op->action == "delete") ? "down" : "noserver");
		}
		recount();
		say("resolved " + std::to_string(n) + " " + side);
		return 0;
	}

	// ---- RomM's games, downloads and BIOS ------------------------------------------------------------------------
	// a verb's error: to stdout, and to the status file for one in the background (its caller reads that)
	int failed(const std::string& line, bool background)
	{
		if (background) status(line);
		say(line);
		return 1;
	}

	// the account and its server for a verb on RomM's library: false, why set (unpaired, down or old), when there is none
	bool library(Account& a, Server& s, std::string& why)
	{
		a = account(card);
		if (a.url.empty() || a.token.empty() || a.deviceId.empty()) { why = "unpaired"; return false; }
		s = server(a, 5000, 30000);
		std::string version;
		if (!versionChecked(s, version, why)) { if (why != "down") why = "old"; return false; }
		return true;
	}

	// the synced system of one of RomM's platforms, when Banana has it
	const System* systemOf(const std::string& slug)
	{
		for (auto& m : SYNCED)
			for (const char* x : m.slugs)
				if (x && slug == x) return findSystem(allSystems(), m.system);
		return nullptr;
	}

	// a field of a tab-separated line: tabs and line ends as spaces
	std::string field(std::string s)
	{
		for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
		return s;
	}

	int listPlatforms()
	{
		Account a;
		Server s;
		std::string why;
		if (!library(a, s, why)) return failed("error platforms " + why, false);
		Platforms p = platforms(s);
		if (p.status == 401) { noteRevoked(); return failed("error platforms revoked", false); }
		if (!p.ok) return failed("error platforms down", false);
		int n = 0;
		for (auto& pl : p.list)
			if (const System* sy = systemOf(pl.slug))
			{
				printf("%ld\t%s\t%s\t%ld\t%s\n", pl.id, field(pl.slug).c_str(), sy->id.c_str(), pl.romCount, field(pl.name).c_str());
				n++;
			}
		log("platforms: %d of RomM's %zu are Banana's systems'", n, p.list.size());
		return 0;
	}

	int listGames(long platform, long offset, const std::string& search)
	{
		Account a;
		Server s;
		std::string why;
		if (!library(a, s, why)) return failed("error roms " + why, false);
		Roms r = roms(s, platform, search, offset < 0 ? 0 : offset, 50, "name");
		if (r.status == 401) { noteRevoked(); return failed("error roms revoked", false); }
		if (!r.ok) return failed("error roms down", false);
		printf("total %ld\n", r.total);
		Games lookup(card, s, false);
		std::vector<std::string> roots = romRoots();
		for (auto& rom : r.items)
		{
			bool on = false;
			if (const System* sy = systemOf(rom.platformSlug))
			{
				on = lookup.recorded(sy->id, rom.id);
				for (auto& root : roots) on = on || (safeName(rom.fsName) && !access((folderOn(root, *sy) + "/" + rom.fsName).c_str(), F_OK));
			}
			printf("%ld\t%lld\t%d\t%s\t%s\n", rom.id, rom.size, on ? 1 : 0, field(rom.fsName).c_str(), field(rom.name).c_str());
		}
		return 0;
	}

	// the downloads' lock (a game's or the BIOS'), not waited for: false when another holds it
	bool lockDownloads()
	{
		makeDir(card.run);
		int fd = open((card.run + "/romm-download.lock").c_str(), O_RDWR | O_CREAT, 0644);
		return fd < 0 || flock(fd, LOCK_EX | LOCK_NB) == 0;	// (held until the helper exits)
	}

	// a download's progress in the status file, at most four times a second
	struct Progress
	{
		std::string tail;
		std::chrono::steady_clock::time_point last;
		void operator()(long long got, long long total, bool now = false)
		{
			auto t = std::chrono::steady_clock::now();
			if (!now && t - last < std::chrono::milliseconds(250)) return;
			last = t;
			status("running " + std::to_string(got) + " " + std::to_string(total) + "\t" + tail);
		}
	};

	int download(long id)
	{
		if (!lockDownloads()) { say("busy"); return 3; }		// (taken before going into the background: the child keeps it)
		status("running 0 0\t\t\t");
		detach();
		Account a;
		Server s;
		std::string why, error;
		if (!library(a, s, why)) return failed("error download " + why, true);
		Awake awake(card);
		RomDetail d = romDetail(s, id);
		if (d.status == 401) { noteRevoked(); return failed("error download revoked", true); }
		if (d.status == 404) return failed("error download notfound", true);
		if (!d.ok) return failed("error download down", true);
		const Rom& r = d.rom;
		const System* sy = systemOf(r.platformSlug);
		if (!sy)
		{
			log("download: %s: RomM's %s isn't one of Banana's systems", r.fsName.c_str(), r.platformSlug.c_str());
			return failed("error download system", true);
		}
		// one file as itself, one in a folder of its own, or several (RomM's zip)
		bool several = r.files.size() > 1, inFolder = several || !r.simple;
		if (!safeName(r.fsName) || (!several && inFolder && (r.files.empty() || !safePath(r.files[0].path))))
		{
			log("download: %ld: a name that can't go on the card (%s)", id, r.fsName.c_str());
			return failed("error download name", true);
		}
		Target t = target(card);
		std::string note = t.fellBack ? "main card: the second card isn't in" : "";
		if (t.fellBack) log("download: the second card isn't in: to the main card");
		std::string dir = folderOn(t.roms, *sy), final = dir + "/" + r.fsName;
		if (!access(final.c_str(), F_OK)) { log("download: %s is there already", final.c_str()); return failed("error download there " + final, true); }
		// room for it (for several files, the zip and what it unpacks to), checked before anything is written
		long long need = several ? 2 * r.size : r.size, free = freeBytes(dir);
		if (free >= 0 && need > free) return failed("error download space " + std::to_string(need) + " " + std::to_string(free), true);
		if (!makeDir(dir)) { log("download: can't make %s", dir.c_str()); return failed("error download card", true); }

		std::string base = dir + "/.romm-" + std::to_string(id), part = base + (several ? ".zip.part" : ".part"), tmp = base + ".dir";
		Progress progress;
		progress.tail = sy->id + "\t" + field(r.name.empty() ? r.fsName : r.name) + "\t" + note;
		progress(0, r.size, true);
		long long expected = several ? 0 : r.files.size() == 1 ? r.files[0].size : r.size;
		Transfer tr = transfer(part, expected, [&](long long from) {
			return romContent(s, r, part, from, [&](long long got, long long total) { progress(from + got, total ? from + total : r.size); });
		});
		if (tr.status == 401) { noteRevoked(); return failed("error download revoked", true); }
		if (!tr.ok)
		{
			log("download: %s: %s, %s", r.fsName.c_str(), tr.why.c_str(), tr.why == "cut" ? "what came kept to go on from" : "nothing kept");
			return failed("error download " + tr.why, true);
		}

		// into place: renamed whole, a folder through .romm-<id>.dir
		bool ok = true;
		if (several)
		{
			removeTree(tmp);
			if (!unpack(part, tmp, error))
			{
				removeTree(tmp);
				std::string zip = dir + "/" + r.fsName + ".zip";
				if (rename(part.c_str(), zip.c_str()) != 0) zip = part;
				log("download: %s: %s, the zip kept as %s", r.fsName.c_str(), error.c_str(), zip.c_str());
				return failed("error download unpack " + zip, true);
			}
			ok = rename(tmp.c_str(), final.c_str()) == 0;
			if (ok) unlink(part.c_str());
		}
		else if (inFolder)
		{
			std::string inside = tmp + "/" + r.files[0].path;
			ok = makeDir(inside.substr(0, inside.rfind('/'))) && rename(part.c_str(), inside.c_str()) == 0
				&& rename(tmp.c_str(), final.c_str()) == 0;
		}
		else ok = rename(part.c_str(), final.c_str()) == 0;
		if (!ok) { log("download: can't put %s in place: %s", final.c_str(), strerror(errno)); return failed("error download card", true); }
		Games lookup(card, s, false);
		lookup.record(sy->id, r.fsName, r.id);
		status("done download " + std::to_string(id) + " " + final);
		log("download: %s: done, %s", r.fsName.c_str(), final.c_str());
		return 0;
	}

	int bios()
	{
		if (!lockDownloads()) { say("busy"); return 3; }
		status("running 0 0\tbios\t");
		detach();
		Account a;
		Server s;
		std::string why;
		if (!library(a, s, why)) return failed("error bios " + why, true);
		Awake awake(card);
		Platforms p = platforms(s);
		if (p.status == 401) { noteRevoked(); return failed("error bios revoked", true); }
		if (!p.ok) return failed("error bios down", true);
		std::vector<Firmware> all;
		for (auto& pl : p.list)
		{
			if (!systemOf(pl.slug)) continue;
			Firmwares f = firmware(s, pl.id);
			if (f.status == 401) { noteRevoked(); return failed("error bios revoked", true); }
			if (!f.ok) return failed("error bios down", true);
			all.insert(all.end(), f.list.begin(), f.list.end());
		}
		std::string dir = card.card + "/bios";
		int fetched = 0, skipped = 0, failures = 0, done = 0;
		for (auto& fw : all)
		{
			status("running " + std::to_string(done++) + " " + std::to_string(all.size()) + "\tbios\t" + field(fw.fileName));
			if (!safeName(fw.fileName)) { log("bios: %s: a name that can't go on the card", fw.fileName.c_str()); failures++; continue; }
			std::string path = dir + "/" + fw.fileName, part = path + ".part", want = lower(fw.md5);
			std::string have = access(path.c_str(), F_OK) ? "" : fileMd5(path);
			if (!have.empty() && have == want) { skipped++; log("bios: %s: the same as RomM's, skipped", fw.fileName.c_str()); continue; }
			if (!makeDir(dir)) { log("bios: can't make %s", dir.c_str()); return failed("error bios card", true); }
			Transfer tr = transfer(part, fw.size, [&](long long from) { return firmwareContent(s, fw, part, from, nullptr); });
			if (tr.status == 401) { noteRevoked(); return failed("error bios revoked", true); }
			if (!tr.ok && (tr.why == "down" || tr.why == "cut")) return failed("error bios down", true);
			if (!tr.ok) { log("bios: %s: %s", fw.fileName.c_str(), tr.why.c_str()); failures++; continue; }
			std::string got = fileMd5(part);
			if (!want.empty() && got != want)
			{
				log("bios: %s: didn't arrive whole (its MD5 differs from RomM's), dropped", fw.fileName.c_str());
				unlink(part.c_str());
				failures++;
				continue;
			}
			// (RomM gave no MD5: the same bytes as the card's are skipped all the same)
			if (!have.empty() && have == got) { unlink(part.c_str()); skipped++; log("bios: %s: the same as RomM's, skipped", fw.fileName.c_str()); continue; }
			if (!have.empty())
			{
				if (rename(path.c_str(), (path + ".bak").c_str()) != 0)
				{
					log("bios: can't keep %s as .bak", path.c_str());
					unlink(part.c_str());
					failures++;
					continue;
				}
				log("bios: %s differs from RomM's, kept as %s.bak", fw.fileName.c_str(), fw.fileName.c_str());
			}
			if (rename(part.c_str(), path.c_str()) != 0) { log("bios: can't put %s in place", path.c_str()); failures++; continue; }
			fetched++;
			log("bios: %s: fetched", fw.fileName.c_str());
		}
		status("done bios " + std::to_string(fetched) + " " + std::to_string(skipped) + " " + std::to_string(failures));
		log("bios: done: %d fetched, %d skipped, %d failed", fetched, skipped, failures);
		return 0;
	}

	// ---- the rest ------------------------------------------------------------------------------------------------
	int check()
	{
		Account a = account(card);
		if (a.url.empty()) { say("unpaired"); return 1; }
		Server s = server(a, 3000, 8000);
		std::string version, why;
		if (!versionChecked(s, version, why)) { say(why); return 1; }
		if (a.token.empty() || a.deviceId.empty()) { say("unpaired"); return 1; }
		long st = getDevice(s, a.deviceId);
		if (st == 401 || st == 403 || st == 404) { say("revoked"); return 1; }
		if (st != 200) { say("down"); return 1; }
		say((versionOk(version) == NEWER ? "newer " : "ok ") + version);
		return 0;
	}

	int logout()
	{
		stopPairing();
		Account a = account(card);
		if (!a.url.empty() && !a.token.empty() && !a.deviceId.empty())
		{
			long st = deleteDevice(server(a, 1500, 5000), a.deviceId);
			if (st == 200 || st == 204 || st == 404) log("logout: the device is off RomM");
			else if (st == 401) log("logout: RomM no longer took the token");
			else log("logout: the device couldn't be deleted on RomM (%s), forgotten here all the same",
				st ? std::to_string(st).c_str() : "no server");
		}
		std::string error;
		if (!forget(card, error)) { log("logout: %s", error.c_str()); say("logout-failed card"); return 1; }
		status("unpaired");
		say("logged-out");
		return 0;
	}
}

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);
	std::string tz = setting(card, "timezone", "");
	if (!tz.empty()) { setenv("TZ", tz.c_str(), 1); tzset(); }
	makeDir(card.logs);
	logTo(card.logs + "/romm.log");
	verb = argc > 1 ? argv[1] : "";
	if (verb == "check") return check();
	if (verb == "pair") return pair();
	if (verb == "logout") return logout();
	if (verb == "pull" && argc == 4) return pull(argv[2], argv[3]);
	if ((verb == "push" || verb == "pending") && argc == 6)
		return (verb == "push" ? push : keepPending)(argv[2], argv[3], atoll(argv[4]), atoll(argv[5]));
	if (verb == "sync" && argc == 2) return syncAll();
	if (verb == "resolve" && argc == 5 && (!strcmp(argv[3], "local") || !strcmp(argv[3], "server")))
		return resolve(atoi(argv[2]), argv[3], argv[4]);
	if (verb == "platforms" && argc == 2) return listPlatforms();
	if (verb == "roms" && (argc == 4 || argc == 5)) return listGames(atol(argv[2]), atol(argv[3]), argc == 5 ? argv[4] : "");
	if (verb == "download" && argc == 3 && atol(argv[2]) > 0) return download(atol(argv[2]));
	if (verb == "bios" && argc == 2) return bios();
	fprintf(stderr, "usage: romm check | pair | logout | pull <system> <rom> | push|pending <system> <rom> <start> <end> | sync"
		" | resolve <n> local|server <rom> | platforms | roms <platform id> <offset> [search] | download <rom id> | bios\n");
	return 2;
}
