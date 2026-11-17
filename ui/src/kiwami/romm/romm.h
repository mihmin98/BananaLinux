#pragma once
// banana-sync (kiwami): the fork's RomM additions (0039, .workspace/0039-romm-home-server/), header-only so
// banana-sync's build recipe stays upstream's (ui/Makefile's kiwami: line names these headers as dependencies). Its
// hooks are in upstream's sync.cpp (the pairing's scopes, the kiwami verb, dropServerLists()) and sync_romm.cpp
// (kiwamiFolder(), the platforms' mapping exported; Romm::list()'s pages and the ids it records, lookup.h; the card's
// ROMs under another name found by their contents, and onCard()'s first line for them, hashes.h; the game list kept on
// the card and brought up to date, listcache.h, whose pages Pages hands list() in a sync of every system):
//   banana-sync kiwami bios list           BIOS files of RomM's for the systems Banana has (bios.h)
//   banana-sync kiwami bios get <id>...    those downloaded into /data/bios (and the DS's into DraStic's folder)
// Each prints its lines on stdout, the last one the result ("ok ..." or "error <why>"), for Settings' pages to read.
// They don't write sync.log, so the last sync's report stays. The fork's own state on the card is
// banana/kiwami-romm.json (stateFile(), State) and banana/kiwami-romm-list.jsonl (listFile()), which Forget and a
// pairing with another server drop (dropState()).
#include "../../sync_core.h"
#include "../../sync_romm.h"
#include "../../sync_util.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>

namespace bsync
{
	// sync_romm.cpp's mapping of a RomM platform to Banana's system folder (its ROMM_SLUGS, then its fs_slug when
	// that is one of Banana's folders), "" for a platform Banana has no system for. Defined there by a kiwami: line
	std::string kiwamiFolder(const banana::JVal& p);
}

namespace banana::kiwami::romm
{
	// the scopes the fork asks for beside the rework's, in the pairing's requested_scopes (sync.cpp's hook):
	// firmware.read for the BIOS page, collections.read for a later Get games
	constexpr const char* SCOPES = ", \"firmware.read\", \"collections.read\"";

	inline std::string stateFile() { return bsync::ROOT + "/banana/kiwami-romm.json"; }
	// RomM's game list kept on the card between syncs (listcache.h)
	inline std::string listFile() { return bsync::ROOT + "/banana/kiwami-romm-list.jsonl"; }
	// the server's pairing gone or replaced (dropServerLists()): what the fork kept of it goes too
	inline void dropState() { unlink(stateFile().c_str()); unlink(listFile().c_str()); }

	// RomM's games of a platform, as 0022 asks for them: n from off, by name, with their files (and their hashes) and
	// nothing list() doesn't read; more, the query's own parameters. KIWAMI_ROMM_ROM_ID_INDEX=1 in the environment
	// leaves with_rom_id_index=false out (to measure RomM's paging with its id index on a large library)
	inline std::string romsQuery(long long pid, long long off, long long n, const std::string& more = "")
	{
		const char* idIndex = getenv("KIWAMI_ROMM_ROM_ID_INDEX");
		std::string p = std::to_string(pid);
		return "/api/roms?platform_ids=" + p + "&platform_id=" + p + more + "&limit=" + std::to_string(n) + "&offset=" + std::to_string(off) +
			"&order_by=name&with_files=true&with_char_index=false&with_filter_values=false" +
			(idIndex && std::string(idIndex) == "1" ? "" : "&with_rom_id_index=false") + "&with_provider_metadata=false";
	}
	// what the fork remembers of the paired server between runs (stateFile()). It holds no secret: File sharing shows
	// it, unlike the pairing. A run reads it again just before it writes, so another run's change in between is kept
	struct State
	{
		// a card file's MD5 as hashes.h read it, kept while its size and time are the same
		struct Hash
		{
			long long size = 0, mtime = 0;
			std::string md5;
			bool operator==(const Hash& o) const { return size == o.size && mtime == o.mtime && md5 == o.md5; }
		};
		std::map<std::string, long long> ids;	// a game on either card (roms-relative, as sync --rom names it) -> its RomM id
		std::map<std::string, Hash> hashes;		// a card file (roms-relative) -> its MD5 (hashes.h)
		std::map<long long, std::string> aliases;	// a RomM id -> the card file of its contents under another name (hashes.h)
		static State load()
		{
			State s;
			JPtr j = banana::readJsonFile(stateFile());
			const JVal* ids = j && j->isObj() ? j->get("ids") : nullptr;
			if (ids && ids->isObj())
				for (auto& kv : ids->obj) if (kv.second && kv.second->isNum() && kv.second->n > 0) s.ids[kv.first] = (long long)kv.second->n;
			const JVal* hs = j && j->isObj() ? j->get("hashes") : nullptr;
			if (hs && hs->isObj())
				for (auto& kv : hs->obj)
					if (kv.second && kv.second->isObj() && !kv.second->str("md5").empty())
						s.hashes[kv.first] = { (long long)kv.second->num("size", -1), (long long)kv.second->num("mtime", -1), kv.second->str("md5") };
			const JVal* al = j && j->isObj() ? j->get("aliases") : nullptr;
			if (al && al->isObj())
				for (auto& kv : al->obj)
					if (kv.second && kv.second->isStr() && !kv.second->s.empty() && atoll(kv.first.c_str()) > 0) s.aliases[atoll(kv.first.c_str())] = kv.second->s;
			return s;
		}
		bool save() const
		{
			std::string o = "{\n \"version\": 1,\n \"ids\": {";
			bool first = true;
			for (auto& kv : ids) { o += std::string(first ? "\n  " : ",\n  ") + bsync::jsonStr(kv.first) + ": " + std::to_string(kv.second); first = false; }
			o += "},\n \"hashes\": {";
			first = true;
			for (auto& kv : hashes)
			{
				o += std::string(first ? "\n  " : ",\n  ") + bsync::jsonStr(kv.first) + ": {\"size\": " + std::to_string(kv.second.size) +
					", \"mtime\": " + std::to_string(kv.second.mtime) + ", \"md5\": " + bsync::jsonStr(kv.second.md5) + "}";
				first = false;
			}
			o += "},\n \"aliases\": {";
			first = true;
			for (auto& kv : aliases) { o += std::string(first ? "\n  " : ",\n  ") + "\"" + std::to_string(kv.first) + "\": " + bsync::jsonStr(kv.second); first = false; }
			return bsync::writeAtomic(stateFile(), o + "}\n}\n");
		}
	};

	// State's aliases as this run has them: read once, as onCard() asks for every game, and replaced by byHash()
	inline std::map<long long, std::string>& aliases()
	{
		static bool read = false;
		static std::map<long long, std::string> m;
		if (!read) { read = true; m = State::load().aliases; }
		return m;
	}
	// the card file (roms-relative) of a server game's contents under another name than the game's, while it is there;
	// "" for none (sync_romm.cpp's onCard() then looks by name, as the rework does)
	inline std::string aliasOf(long long id)
	{
		auto& m = aliases();
		if (m.empty()) return "";
		auto at = m.find(id);
		return at == m.end() || !bsync::exists(bsync::romPath(at->second)) ? "" : at->second;
	}

	// the paired RomM, for a command: its pairing read (c) and a backend to ask it through; "" when ready, else why not
	struct Paired
	{
		bsync::Conf c;
		std::unique_ptr<bsync::Backend> b;
		std::string open()
		{
			c = bsync::loadConf();
			if (!c.paired()) return "not paired";
			if (c.backend != "romm") return "not a RomM";
			b = bsync::newRomm(c);
			return "";
		}
	};

	// RomM's platforms that are Banana's systems: platform id -> system folder. "" when read, else why not ("scope"
	// for a token RomM refuses, "unpaired" as the rework's listing says it)
	inline std::string platformsMapped(const bsync::Backend& b, std::map<long long, std::string>& out)
	{
		bsync::Resp r = b.call("GET", "/api/platforms");
		if (r.code == 401) return "unpaired";
		if (r.code == 403) return "scope";
		if (r.code != 200) return "platforms: " + r.error();
		JPtr j = r.json();
		if (!j || !j->isArr()) return "the server's platforms can't be read";
		for (auto& p : j->arr)
		{
			std::string folder = bsync::kiwamiFolder(*p);
			if (!folder.empty()) out[(long long)p->num("id")] = folder;
		}
		return "";
	}

	// the last line of a command: "ok" (with what follows it) or "error <why>", and its exit code
	inline int answer(const std::string& why, const std::string& ok = "ok")
	{
		printf("%s\n", why.empty() ? ok.c_str() : ("error " + why).c_str());
		return why.empty() ? 0 : 1;
	}

	namespace bios
	{
		inline int list();
		inline int get(const std::vector<std::string>& ids);
	}

	// banana-sync kiwami <what> ...: args as main() has them, "kiwami" first
	inline int command(const std::vector<std::string>& args)
	{
		std::string what = args.size() > 1 ? args[1] : "", sub = args.size() > 2 ? args[2] : "";
		if (what == "bios" && sub == "list") return bios::list();
		if (what == "bios" && sub == "get" && args.size() > 3) return bios::get(std::vector<std::string>(args.begin() + 3, args.end()));
		fprintf(stderr, "usage: banana-sync kiwami bios list | bios get <id>...\n");
		return 2;
	}
}

#include "bios.h"
#include "hashes.h"
#include "listcache.h"
#include "lookup.h"
