#pragma once
// banana-sync (kiwami): a ROM on the card under another name than the server's, found by its contents in a sync's
// listing of every system (Romm::list()'s kiwami: line before record()). The rework pairs a card's ROM with a server
// game by its file name alone (onCard()), so a game renamed on the card (a tidier name, another set's) never has its
// save synced. byHash() takes, system by system, the card's files that no server game is named after (archives and
// .m3u left out) and the server's games of one file that aren't archives either, and pairs them by content, from the
// listing itself (no request of its own): by size first (files[]' file_size_bytes), so a card file is read only when a
// server game's file has its very size, then by the MD5 the listing gives. A pair is an alias (State's aliases, the
// server's id -> the card file), which onCard() returns first (sync_romm.cpp's kiwami: line, aliasOf()): the game's
// save syncs in that game's slot, and record() keeps its id for the sync before it (lookup.h). No alias where it can't
// be said for sure: two card files of those contents, two server games of them, or a server game already on the card
// under its own name. Each pair, and each file read that isn't one, goes to sync.log ("  by hash: <card file> is
// <server file>", "  by hash: <card file> not matched: <why>"). A file's MD5 is kept by its path, size and time
// (State's hashes), so it is read once until it changes. Not for one game's run nor those waiting to be sent (they
// list one system, or the games of a few), nor Get games' refresh, which has no sync's status (only a sync's
// "running" one starts it): their aliases are those the last Sync now found.
#include "romm.h"
#include "../synctimes.h"
#include "../../save_paths.h"
#include <set>
#include <sys/stat.h>

namespace banana::kiwami::romm
{
	// a file whose MD5 RomM takes of what is inside it, never the file's own: sync_romm.cpp's packedName()'s archives
	inline bool packed(const std::string& name)
	{
		std::string e = bsync::extOf(name);
		return e == "zip" || e == "7z" || e == "rar" || e == "tar" || e == "tgz" || e == "tbz2" || e == "txz" || e == "gz" || e == "bz2" ||
			(e == "xz" && bsync::extOf(bsync::stemOf(name)) == "tar");
	}

	// after list() read every system's games (R: sync_romm.cpp's Romm, its roms, romJson, onlyRom, onlyQueued and
	// onCard()): the aliases found again, those no longer found dropped, and the MD5s read kept; written only when they
	// changed
	template <class R> void byHash(R& r)
	{
		if (bsync::dryRun || !r.onlyRom.empty() || r.onlyQueued || banana::kiwami::synctimes::lastStatus().compare(0, 8, "running ") != 0) return;
		auto& al = aliases();
		al.clear();	// (onCard() by name alone from here: which server games have a card file of their own name)
		// the server's games of one file, by system and that file's size: their id, file name and MD5, and whether a
		// card file has their name
		struct Game { long long id; std::string fsName, md5; bool named; };
		std::map<std::string, std::map<long long, std::vector<Game>>> bySize;
		std::map<std::string, std::set<std::string>> names;	// system -> the file names its server games are on the card by
		std::map<long long, long long> sizeOf;	// a game of one file -> that file's size
		for (auto& j : r.romJson)
		{
			const JVal* files = j->get("files");
			if (files && files->isArr() && files->arr.size() == 1 && files->arr[0]->get("file_size_bytes"))
				sizeOf[(long long)j->num("id")] = (long long)files->arr[0]->num("file_size_bytes");
		}
		for (auto& g : r.roms)
		{
			if (!g.entry.empty()) names[g.folder].insert(g.entry);
			auto sz = sizeOf.find(g.id);
			if (g.entry != g.fsName || g.files.size() != 1 || g.files[0].md5.empty() || packed(g.fsName) || sz == sizeOf.end() || sz->second <= 0) continue;
			bySize[g.folder][sz->second].push_back({ g.id, g.fsName, bsync::lower(g.files[0].md5), !r.onCard(g).empty() });
		}
		// the card's files named after none of their system's server games, with a server game's file of their size
		// (onCard() above read each of those systems' folders into cardIndex)
		struct File { std::string rel, path; long long size, mtime; const std::vector<Game>* games; };
		std::vector<File> files;
		for (auto& sys : bySize)
		{
			auto ci = r.cardIndex.find(sys.first);
			if (ci == r.cardIndex.end()) continue;
			for (auto& kv : ci->second)
			{
				if (names[sys.first].count(kv.first) || packed(kv.first) || bsync::extOf(kv.first) == "m3u") continue;
				std::string p = bsync::romPath(kv.second);
				struct stat st;
				if (stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
				auto at = sys.second.find((long long)st.st_size);
				if (at != sys.second.end()) files.push_back({ kv.second, p, (long long)st.st_size, (long long)st.st_mtime, &at->second });
			}
		}
		State was = State::load();
		std::map<std::string, State::Hash> hashes;
		std::map<long long, std::vector<std::string>> of;	// a server game -> the card files of its contents
		std::map<long long, std::string> fsNames;
		for (auto& f : files)
		{
			banana::kiwami::synctimes::tick(bsync::status, "Matching by hash", files.size(), f.rel);
			auto h = was.hashes.find(f.rel);
			State::Hash cur{ f.size, f.mtime, h != was.hashes.end() ? h->second.md5 : "" };
			if (h == was.hashes.end() || !(h->second == cur))
			{
				bsync::say("  by hash: reading %s", f.rel.c_str());
				cur.md5 = bsync::hashFile(f.path, "md5");
				if (cur.md5.empty()) { bsync::say("  by hash: %s not matched: it can't be read", f.rel.c_str()); continue; }
			}
			hashes[f.rel] = cur;
			std::vector<const Game*> same;
			for (auto& g : *f.games) if (g.md5 == cur.md5) same.push_back(&g);
			if (same.empty()) bsync::say("  by hash: %s not matched: no server game of its system has its contents", f.rel.c_str());
			else if (same.size() > 1)
				bsync::say("  by hash: %s not matched: two of the server's games have its contents (%s, %s)", f.rel.c_str(), same[0]->fsName.c_str(), same[1]->fsName.c_str());
			else if (same[0]->named)
				bsync::say("  by hash: %s not matched: %s is on the card under its own name", f.rel.c_str(), same[0]->fsName.c_str());
			else { of[same[0]->id].push_back(f.rel); fsNames[same[0]->id] = same[0]->fsName; }
		}
		std::map<long long, std::string> found;
		for (auto& kv : of)
		{
			if (kv.second.size() > 1)
			{
				bsync::say("  by hash: %s and %s not matched: both are %s", kv.second[0].c_str(), kv.second[1].c_str(), fsNames[kv.first].c_str());
				continue;
			}
			bsync::say("  by hash: %s is %s", kv.second[0].c_str(), fsNames[kv.first].c_str());
			found[kv.first] = kv.second[0];
		}
		al = found;
		State s = State::load();
		if (s.hashes == hashes && s.aliases == found) return;
		s.hashes = hashes;
		s.aliases = found;
		s.save();
	}
}
