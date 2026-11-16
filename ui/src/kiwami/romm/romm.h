#pragma once
// banana-sync (kiwami): the fork's RomM additions (0039, .workspace/0039-romm-home-server/), header-only so
// banana-sync's build recipe stays upstream's (ui/Makefile's kiwami: line names these headers as dependencies). Its
// hooks are in upstream's sync.cpp (the pairing's scopes, the kiwami verb, dropServerLists()) and sync_romm.cpp
// (kiwamiFolder(), the platforms' mapping exported):
//   banana-sync kiwami bios list           BIOS files of RomM's for the systems Banana has (bios.h)
//   banana-sync kiwami bios get <id>...    those downloaded into /data/bios (and the DS's into DraStic's folder)
// Each prints its lines on stdout, the last one the result ("ok ..." or "error <why>"), for Settings' pages to read.
// They don't write sync.log, so the last sync's report stays. The fork's own state on the card is
// banana/kiwami-romm.json (stateFile()), which Forget and a pairing with another server drop (dropState()).
#include "../../sync_core.h"
#include "../../sync_romm.h"
#include "../../sync_util.h"
#include <cstdio>
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
	// the server's pairing gone or replaced (dropServerLists()): what the fork kept of it goes too
	inline void dropState() { unlink(stateFile().c_str()); }

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
