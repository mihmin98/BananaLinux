#pragma once
// banana-sync (kiwami): one game's save synced before it starts (sync --rom <path> --auto launch), and after it,
// without listing its whole system. The rework's Romm::list() lists every game of that game's system to find its one
// ROM: on a system of 1000+ games, all of their JSON, up to the 20 s cap before a game. Its page loop takes its pages
// from Pages (sync_romm.cpp's kiwami: hook), which for such a run first asks RomM for the game alone:
//   - by its RomM id, remembered (State's ids, by the card file's roms-relative path, as --rom names it):
//     GET /api/roms/<id>, taken when the game is still on that platform under the card file's name (or the card file
//     is still its alias, a game of its contents under another name, hashes.h);
//   - else by RomM's name search on the platform, GET /api/roms?search_term=<the card file's stem> (RomM 5.4.1's
//     backend/handler/database/roms_handler.py, _filter_by_search_term(): each word of it in the game's name, its
//     file name or its other titles, case aside), taken when it holds a game of the card file's name.
// Either answer goes to the loop as a page of its own ({"items": [...], "total": n}), so the rework's own matching (by
// name, then by content) runs on it as on a listed page. With neither, the system is listed in pages as 0022 has it,
// and the outcome is the rework's: a game renamed or deleted on the server, a name RomM's search doesn't find.
// record() (list()'s hook before listed = true, after byHash()'s) keeps the ids: after a listing of every system, those
// of the games on either card (by name, or by contents under another name); after one game's, that game's, or none
// when the server no longer has it.
// KIWAMI_ROMM_ROM_ID_INDEX=1 in the environment leaves with_rom_id_index=false out of the queries (to measure RomM's
// paging with its id index on a large library).
#include "romm.h"
#include "../syncmem.h"
#include "../../save_paths.h"
#include <cstdlib>

namespace banana::kiwami::romm
{
	// the games a search asks for: fewer than a page, so list()'s loop takes its answer as a platform's last page
	constexpr int SEARCH = 50;
	static_assert(SEARCH < banana::kiwami::syncmem::PAGE, "a search's answer has to end list()'s page loop");

	// RomM's games of a platform, as 0022 asks for them: n from off, by name, with their files (and their hashes) and
	// nothing list() doesn't read; more, the query's own parameters
	inline std::string romsQuery(long long pid, long long off, long long n, const std::string& more = "")
	{
		const char* idIndex = getenv("KIWAMI_ROMM_ROM_ID_INDEX");
		std::string p = std::to_string(pid);
		return "/api/roms?platform_ids=" + p + "&platform_id=" + p + more + "&limit=" + std::to_string(n) + "&offset=" + std::to_string(off) +
			"&order_by=name&with_files=true&with_char_index=false&with_filter_values=false" +
			(idIndex && std::string(idIndex) == "1" ? "" : "&with_rom_id_index=false") + "&with_provider_metadata=false";
	}

	// the file list() makes of a listed game in its folder: its one file, a disc set's .m3u on the PlayStation, none
	// for another game of several files (the menu can't start it). As list() has it
	inline std::string entryOf(const JVal& it, const std::string& folder)
	{
		std::string fsName = it.str("fs_name");
		const JVal* files = it.get("files");
		bool multi = it.flag("has_multiple_files") || (files && files->isArr() && files->arr.size() > 1);
		return !multi ? fsName : folder == "psx" ? fsName + ".m3u" : "";
	}

	// a platform's pages for list(): for one game's run whose game is in this platform's folder, the game alone when
	// RomM has it under the card file's name, else (and for every other run) 0022's pages
	struct Pages
	{
		const bsync::Backend& b;
		const std::string onlyRom, folder;
		const long long pid;
		bool one = false;
		Pages(const bsync::Backend& backend, const std::string& onlyRom_, bool sendOk, long long pid_, const std::string& folder_)
			: b(backend), onlyRom(onlyRom_), folder(folder_), pid(pid_)
		{
			// (the run's platforms are this game's folder's already then: list() leaves the others out)
			std::string rf = onlyRom.empty() || onlyRom[0] == '/' || !sendOk ? "" : bsync::romFolder(folder);
			one = !rf.empty() && onlyRom.compare(0, rf.size() + 1, rf + "/") == 0;
		}
		// the page from off: the game alone first (one answer, which ends the loop), else 0022's
		bsync::Resp fetch(long long off)
		{
			bsync::Resp r;
			if (off == 0 && one && (byId(r) || bySearch(r))) return r;
			return b.call("GET", romsQuery(pid, off, banana::kiwami::syncmem::PAGE));
		}

	private:
		// a game of this platform under the card file's name, or whose contents the card file is under another name
		// (an alias the last Sync now found, hashes.h)
		bool exact(const JVal& it) const
		{
			return it.isObj() && (long long)it.num("platform_id") == pid &&
				(entryOf(it, folder) == bsync::baseOf(onlyRom) || aliasOf((long long)it.num("id")) == onlyRom);
		}
		bool byId(bsync::Resp& out)
		{
			State s = State::load();
			auto at = s.ids.find(onlyRom);
			if (at == s.ids.end()) return false;
			bsync::Resp r = b.call("GET", "/api/roms/" + std::to_string(at->second));
			if (r.code == 0) { out = r; return true; }	// (no answer: the loop says so, as it would of the listing's)
			JPtr j = r.code == 200 ? r.json() : nullptr;
			if (!j || !exact(*j))
			{
				bsync::say("  RomM's game %lld is no longer %s (%s): looked for by name", at->second, onlyRom.c_str(), r.code == 200 ? "another file" : std::to_string(r.code).c_str());
				return false;
			}
			bsync::say("  %s: RomM's game %lld, as remembered", onlyRom.c_str(), at->second);
			r.body = "{\"items\": [" + r.body + "], \"total\": 1}";
			out = r;
			return true;
		}
		bool bySearch(bsync::Resp& out)
		{
			std::string stem = bsync::stemOf(bsync::baseOf(onlyRom));
			bsync::Resp r = b.call("GET", romsQuery(pid, 0, SEARCH, "&search_term=" + bsync::urlEsc(stem)));
			if (r.code == 0) { out = r; return true; }
			JPtr page = r.code == 200 ? r.json() : nullptr;
			const JVal* items = page ? (page->isArr() ? page.get() : page->get("items")) : nullptr;
			long long found = 0;
			if (items && items->isArr())
				for (auto& it : items->arr) if (it && exact(*it)) { found = (long long)it->num("id"); break; }
			if (!found)
			{
				bsync::say("  %s: not found by RomM's search (%s), its system listed", onlyRom.c_str(), r.code == 200 ? "no game of its name" : std::to_string(r.code).c_str());
				return false;
			}
			bsync::say("  %s: RomM's game %lld, found by its name", onlyRom.c_str(), found);
			out = r;
			return true;
		}
	};

	// after list() read the server's games (R: sync_romm.cpp's Romm, its roms, onlyRom and onCard()): the RomM ids of
	// the games on either card, kept for the sync before a game. A run of one game records that game's id, or drops
	// it when the server no longer has that game under the card file's name; any other run lists every system and
	// records every game found on a card, those no longer found dropped. Written only when they changed
	template <class R> void record(R& r)
	{
		if (bsync::dryRun) return;
		if (!r.onlyRom.empty())
		{
			long long id = 0;
			for (auto& g : r.roms) if (r.onCard(g) == r.onlyRom) { id = g.id; break; }
			State s = State::load();
			auto at = s.ids.find(r.onlyRom);
			if (id ? at != s.ids.end() && at->second == id : at == s.ids.end()) return;
			if (id) s.ids[r.onlyRom] = id; else s.ids.erase(r.onlyRom);
			s.save();
			return;
		}
		std::map<std::string, long long> ids;
		for (auto& g : r.roms)
		{
			std::string here = r.onCard(g);
			if (!here.empty()) ids[here] = g.id;
		}
		// (the card's ROMs are read again when the saves look at them: a game Get games brings meanwhile is there then)
		r.cardIndex.clear();
		State s = State::load();
		if (s.ids == ids) return;
		s.ids = ids;
		s.save();
	}
}
