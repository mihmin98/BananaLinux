#pragma once
// banana-sync (kiwami): games sent to this handheld from RomM's web page (Home server > Installs from RomM, the setting
// kiwami_romm_installs, off by default). RomM 5.4.1 (backend/endpoints/device/install.py, handler/device_install/)
// queues a request for a game on a device only when the device's capabilities hold remote_install: true (policy.py
// accepts_remote_install()), and keeps it while it is pending or taken, two days at most
// (DEVICE_INSTALL_REQUEST_TTL_DAYS). The device claims them (POST /api/devices/<id>/installs/claim: the pending ones
// taken, and every one it holds taken returned, again too) and reports each (PUT /api/devices/<id>/installs/<request
// id>: done, already_installed, or failed with a reason of 500 characters at most), which ends it; a request gone
// (expired, cancelled in the web page) answers 404, one not taken 409. Claiming and reporting need the token to be
// the device's own (device_only): RomM's device pairing makes one, a client token's pairing code one of no device,
// which gets a 403.
//   kiwami installs on|off: the capability set (PUT /api/devices/<id>), and with on a claim as the check that this
//     pairing can take installs (the requests it takes come back with the next claim; a 403 turns the capability
//     back off). The last line "ok" or "error <why>", for Settings' row.
//   claim(): in a run of the games (Sync now, Get games' run, Wi-Fi's), each request claimed goes on Get games' list
//     (homesync-get.txt), so the same run brings it, with its space check, its .part resumes and "Games go to". A game
//     on either card already is reported already_installed, and one the menu can't start (not in the run's manifest:
//     a system Banana doesn't have, a game of several files on a cartridge system) failed.
//   report(): at the end of the run, each request claimed: done once its game's files are on a card; failed when
//     there was no space for it (taken off the list), or when its owner took it off Get games' list on the handheld;
//     else still taken, its game left on the list for the next sync to try again (a download that failed, the
//     second SD card out), and claimed again then.
//   wanted(): Wi-Fi's run asks whether there's a request this handheld hasn't claimed (doSync's hook then runs one of
//     the games alone, when Auto sync doesn't sync then).
// The requests claimed are kept in kiwami-romm.json (State's installs), with the device the capability was sent for.
#include "romm.h"
#include "../../get_list.h"

namespace banana::kiwami::romm
{
	namespace installs
	{
		inline bool on(const std::map<std::string, std::string>& set)
		{
			auto i = set.find("kiwami_romm_installs");
			return i != set.end() && i->second == "on";
		}
		// a RomM with installs: 5.4 or newer (as the last heartbeat said; none known taken as new, as the rework's slots
		// are), and this card's device in it
		inline bool server(const bsync::Conf& c)
		{
			return c.backend == "romm" && !c.device.empty() && (c.serverVersion.empty() || bsync::versionAtLeast(c.serverVersion, 5, 4, 0));
		}
		inline std::string device(const bsync::Conf& c) { return "/api/devices/" + bsync::urlEsc(c.device); }
		inline bsync::Resp capability(const bsync::Backend& b, bool on)
		{
			std::string body = std::string("{\"capabilities\": {\"remote_install\": ") + (on ? "true" : "false") + "}}";
			return b.call("PUT", device(b.c), &body);
		}
		inline bsync::Resp claimAll(const bsync::Backend& b)
		{
			std::string none;
			return b.call("POST", device(b.c) + "/installs/claim", &none);
		}
		// why RomM didn't take a call
		inline std::string why(const bsync::Resp& r)
		{
			return r.code == 403 ? "this pairing isn't the handheld's own: pair again from it (Server > Pair)"
				: r.code == 401 ? "unpaired" : r.error();
		}
		// at most n bytes of s, not cut inside a character
		inline std::string cut(const std::string& s, size_t n)
		{
			if (s.size() <= n) return s;
			while (n > 0 && ((unsigned char)s[n] & 0xc0) == 0x80) n--;
			return s.substr(0, n);
		}
		// a game's files all on a card: this one or the second, or the second's while it's out (the state has them there)
		inline bool onCard(const std::vector<std::string>& files, const bsync::Conf& c)
		{
			std::string sd2 = bsync::sd2Roms();
			for (auto& f : files)
				if (!bsync::exists(bsync::ROMS + "/" + f) && (sd2.empty() ? !c.where.count(f) : !bsync::exists(sd2 + "/" + f))) return false;
			return !files.empty();
		}
		// a manifest file's RomM ids, from its URL: the game's (rom) for a game of one file, a file's (file) for a disc
		// of a set (sync_romm.cpp's manifest(): /api/roms/<rom id>/content/<name>, /api/roms/<file id>/files/content/<name>)
		inline void idsOf(const std::string& url, const std::string& server, long long& rom, long long& file)
		{
			rom = file = 0;
			std::string p = server + "/api/roms/";
			if (url.compare(0, p.size(), p) != 0) return;
			size_t i = p.size(), e = url.find('/', i);
			if (e == std::string::npos || e == i || url.find_first_not_of("0123456789", i) != e) return;
			long long id = atoll(url.substr(i, e - i).c_str());
			if (url.compare(e, 9, "/content/") == 0) rom = id;
			else if (url.compare(e, 15, "/files/content/") == 0) file = id;
		}
		// a game's files the card needs (not its cover), the first its key on Get games' list (sync.cpp's gameKey())
		inline std::vector<std::string> filesOf(const bsync::MGame& g)
		{
			std::vector<std::string> out;
			for (auto& f : g.files) if (!f.media) out.push_back(f.path);
			return out;
		}
		inline bsync::Resp put(const bsync::Backend& b, const std::string& id, const std::string& status, const std::string& reason = "")
		{
			std::string body = "{\"status\": " + bsync::jsonStr(status) + (reason.empty() ? "" : ", \"reason\": " + bsync::jsonStr(cut(reason, 500))) + "}";
			return b.call("PUT", device(b.c) + "/installs/" + bsync::urlEsc(id), &body);
		}
		// a request ended (reported, or gone from RomM): said, true; else said, false (reported again next time)
		inline bool told(const bsync::Resp& r, const std::string& what, const std::string& status, const std::string& reason = "")
		{
			if (r.code == 200)
			{
				bsync::say("  installs: %s reported %s%s", what.c_str(), status.c_str(), reason.empty() ? "" : (": " + reason).c_str());
				return true;
			}
			if (r.code == 404 || r.code == 409)
			{
				bsync::say("  installs: %s: RomM's request is gone (it ran out, or was cancelled on its page)", what.c_str());
				return true;
			}
			bsync::say("  installs: %s not reported %s (%s), next time", what.c_str(), status.c_str(), why(r).c_str());
			return false;
		}

		// banana-sync kiwami installs on|off
		inline int command(bool on)
		{
			Paired p;
			std::string e = p.open();
			if (!e.empty()) return answer(e);
			if (p.c.device.empty()) return answer("this handheld isn't a RomM device yet: Sync now first");
			if (!server(p.c)) return answer("RomM " + p.c.serverVersion + " has no installs (5.4 or newer)");
			bsync::Resp r = capability(*p.b, on);
			if (r.code != 200) return answer(why(r));
			if (on)
			{
				bsync::Resp k = claimAll(*p.b);
				if (k.code != 200)
				{
					capability(*p.b, false);
					State s = State::load();
					if (!s.installsDevice.empty()) { s.installsDevice.clear(); s.save(); }
					return answer(why(k));
				}
			}
			State s = State::load();
			s.installsDevice = on ? p.c.device : "";
			if (!s.save()) return answer("the setting can't be kept on the card");
			return answer("");
		}
	}

	// the requests sent from RomM's page claimed, each game put on Get games' list for this run to bring (doSync's
	// hook, in a run of the games of every system, after the server answered its heartbeat)
	inline void claim(bsync::Backend& b, bsync::Conf& c, const std::map<std::string, std::string>& set)
	{
		using namespace installs;
		if (!on(set) || !server(c)) return;
		State s = State::load();
		// (the capability once a device: a pairing made again, or the device registered again, gets it too)
		if (s.installsDevice != c.device)
		{
			bsync::Resp r = capability(b, true);
			if (r.code != 200) { bsync::say("  installs: RomM not told this handheld takes them: %s", why(r).c_str()); return; }
			bsync::say("  installs: RomM told this handheld takes games sent from its page");
			State n = State::load();
			n.installsDevice = c.device;
			n.save();
		}
		bsync::Resp r = claimAll(b);
		if (r.code != 200) { bsync::say("  installs: none claimed: %s", why(r).c_str()); return; }
		JPtr j = r.json();
		if (!j || !j->isArr()) { bsync::say("  installs: RomM's claim can't be read"); return; }
		std::vector<const JVal*> fresh;	// (those claimed before are on Get games' list already, or were taken off it)
		for (auto& x : j->arr) if (x && x->isObj() && !x->str("id").empty() && !s.installs.count(x->str("id"))) fresh.push_back(x.get());
		if (fresh.empty()) return;
		bsync::Manifest m;
		std::string err;
		if (!b.manifest(m, {}, err)) { bsync::say("  installs: %zu claimed, left for the next sync: the server's games can't be listed (%s)", fresh.size(), err.c_str()); return; }
		// (each game found by its first file's URL: the game's own id, or a disc's file id)
		std::map<long long, const bsync::MGame*> byRom, byFile;
		for (auto& sy : m.systems)
			for (auto& g : sy.games)
				for (auto& f : g.files)
				{
					if (f.media) continue;
					long long rom, file;
					idsOf(f.url, c.server, rom, file);
					if (rom) byRom[rom] = &g;
					if (file) byFile[file] = &g;
					break;
				}
		std::vector<std::string> add;
		std::map<std::string, State::Install> took;
		for (const JVal* x : fresh)
		{
			std::string id = x->str("id");
			long long rom = (long long)x->num("rom_id", 0);
			const bsync::MGame* g = nullptr;
			auto at = byRom.find(rom);
			if (at != byRom.end()) g = at->second;
			if (const JVal* fs = x->get("file_ids"))
				if (!g && fs->isArr()) for (auto& f : fs->arr) { auto bf = byFile.find((long long)f->n); if (bf != byFile.end()) { g = bf->second; break; } }
			std::string what = "game " + std::to_string(rom);
			std::vector<std::string> files = g ? filesOf(*g) : std::vector<std::string>();
			if (files.empty())
			{
				std::string no = !g ? "Banana can't run this game: its system isn't one of Banana's, or it's a game of several files on a cartridge system"
					: "Banana can't run this game: it has no file to download";
				told(put(b, id, "failed", no), what, "failed", no);
				continue;
			}
			what = files[0];
			if (onCard(files, c) || !aliasOf(rom).empty()) { told(put(b, id, "already_installed"), what, "already_installed"); continue; }
			add.push_back(files[0]);
			took[id] = { files[0], rom, files };
			bsync::say("  installs: %s sent from RomM, on Get games' list", files[0].c_str());
		}
		if (took.empty()) return;
		// (the list written first: a request not kept here is claimed again, and decided again, by the next sync)
		if (!bsync::editGetList(bsync::ROOT + "/banana", bsync::RUN, add, {})) { bsync::say("  installs: Get games' list can't be written, left for the next sync"); return; }
		State n = State::load();
		for (auto& kv : took) n.installs[kv.first] = kv.second;
		if (!n.save()) bsync::say("  installs: what was claimed can't be kept on the card (%s)", stateFile().c_str());
	}

	// at the end of a run, the requests claimed reported as they stand (doSync's hook)
	inline void report(bsync::Conf& c)
	{
		using namespace installs;
		State s = State::load();
		if (s.installs.empty() || c.device.empty()) return;
		auto b = bsync::newRomm(c);
		auto v = bsync::readListLines(bsync::ROOT + "/banana/homesync-get.txt");
		std::set<std::string> listed(v.begin(), v.end());
		std::map<std::string, std::string> failed;
		for (auto& l : bsync::readListLines(bsync::ROOT + "/banana/homesync-get-failed.txt"))
		{
			size_t t = l.find('\t');
			if (t != std::string::npos) failed[l.substr(0, t)] = l.substr(t + 1);
		}
		std::set<std::string> ended, drop;
		int waiting = 0;
		for (auto& kv : s.installs)
		{
			const State::Install& in = kv.second;
			std::string status, reason;
			if (onCard(in.files, c)) status = "done";
			else if (!listed.count(in.key)) { status = "failed"; reason = "taken off Get games on the handheld"; }
			else
			{
				// (no space: a lasting reason, said to RomM; anything else, a download that failed or the second SD card
				// out, left to the next sync, as the rework leaves the game on its list)
				auto f = failed.find(in.key);
				if (f == failed.end() || f->second.compare(0, 16, "not enough space") != 0) { waiting++; continue; }
				status = "failed";
				reason = f->second;
			}
			if (!told(put(*b, kv.first, status, reason), in.key, status, reason)) continue;
			ended.insert(kv.first);
			if (status == "failed" && listed.count(in.key)) drop.insert(in.key);
		}
		if (waiting) bsync::say("  installs: %d waiting on Get games' list", waiting);
		if (!drop.empty() && !bsync::editGetList(bsync::ROOT + "/banana", bsync::RUN, {}, drop)) bsync::say("  installs: Get games' list can't be written");
		if (ended.empty()) return;
		State n = State::load();
		for (auto& id : ended) n.installs.erase(id);
		if (!n.save()) bsync::say("  installs: what was reported can't be kept on the card (%s)", stateFile().c_str());
	}

	// Wi-Fi's run: a request on RomM this handheld hasn't claimed (pending, or taken by Settings' check), with installs on
	inline bool wanted(bsync::Conf& c, const std::map<std::string, std::string>& set)
	{
		using namespace installs;
		if (!on(set) || !server(c)) return false;
		auto b = bsync::newRomm(c);
		bsync::Resp r = b->call("GET", device(c) + "/installs");
		JPtr j = r.code == 200 ? r.json() : nullptr;
		if (!j || !j->isArr()) return false;
		State s = State::load();
		for (auto& x : j->arr)
			if (x && x->isObj() && (x->str("status") == "pending" || (x->str("status") == "taken" && !s.installs.count(x->str("id"))))) return true;
		return false;
	}
}
