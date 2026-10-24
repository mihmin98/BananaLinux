// The RomM helper (kiwami), /usr/libexec/kiwami/romm: everything of the fork's RomM client that talks to the server,
// for Settings' RomM pages and the wrapper around the games (romm-game). One verb a run:
//   romm check     one line to stdout: ok <version>, newer <version>, old <version>, unpaired, revoked or down;
//                  exits 0 for ok and newer
//   romm pair      pairs with accounts.conf's romm_url by RomM's device flow: prints (and puts in the status file)
//                  "pairing <code>\t<the page to approve it on>", then goes on in the background, asking RomM every
//                  few seconds until the user approves it (the token and the device written to accounts.conf, the
//                  status "paired <version>"), denies it or lets it expire ("pair-failed denied|expired"). A second
//                  pair stops the first. Failing at once, it prints the pair-failed line and exits 1
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
// The saves are changed under one lock (<run>/romm-saves.lock): the pull waits for it within its 3 s, the push up to 60 s.
// The status file is <run>/romm.status, one line; the log <card>/banana/logs/romm.log (http.h), which the token never
// reaches: the helper reads it from accounts.conf, never from its command line. The paths: rommcard.h.
// KIWAMI_ROMM_SYNC=1 keeps it in the foreground (the tests); KIWAMI_MODEL moves /proc/device-tree/model.
#include "api.h"
#include "games.h"
#include "http.h"
#include "saves.h"
#include "../conf.h"
#include "../rommcard.h"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/file.h>
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
				if (paired(card, t.token, t.deviceId, error)) result = "paired " + version;
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
		if (g.save.ds) ndsSaves(card, g.rom);
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
	fprintf(stderr, "usage: romm check | pair | logout | pull <system> <rom> | push|pending <system> <rom> <start> <end>\n");
	return 2;
}
