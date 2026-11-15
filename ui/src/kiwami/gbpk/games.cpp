// banana-ui (kiwami): the Game Boy Pokémon companion's games: the ROM file, the data files, and which game a ROM is
// (games.h). PokeDaisy tells its releases by the whole file's SHA-1 too (GameTitles.kt); the titles and SHA-1s here
// are pret's (each repository's Makefile and roms.sha1), checked equal to PokeDaisy's by the extraction script.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include "../json.h"
#include "../ra7z.h"
#include "../../nds/nitro.h"
#include "../../zip.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

namespace banana::kiwami::gbpk
{
	namespace
	{
		const uint64_t MAX_ROM = 8u << 20;	// the largest Game Boy cartridge (MBC5: 512 banks of 16 KB)

		bool endsWithI(const std::string& s, const char* suf)
		{
			size_t n = strlen(suf);
			if (s.size() < n) return false;
			for (size_t i = 0; i < n; i++) if (tolower((unsigned char)s[s.size() - n + i]) != suf[i]) return false;
			return true;
		}
		std::string lower(std::string s)
		{
			for (char& c : s) c = (char)tolower((unsigned char)c);
			return s;
		}
		// the game in an archive: the first .gb or .gbc in name order, letters' case aside, as RetroArch takes it
		int pickGame(const std::vector<rnds::ZipMember>& ms)
		{
			int best = -1;
			for (size_t i = 0; i < ms.size(); i++)
				if ((endsWithI(ms[i].name, ".gb") || endsWithI(ms[i].name, ".gbc")) && ms[i].size <= MAX_ROM &&
					(best < 0 || lower(ms[i].name) < lower(ms[best].name)))
					best = (int)i;
			return best;
		}

		// (the retail games first: a title they share with a hack names them first)
		const struct { const char* key; nds::GameId id; } GAMES[] = {
			{ "red", RED }, { "blue", BLUE }, { "yellow", YELLOW }, { "gold", GOLD }, { "silver", SILVER }, { "crystal", CRYSTAL },
			{ "crystal-legacy", CRYSTAL_LEGACY }, { "yellow-legacy", YELLOW_LEGACY }, { "purered", PURERED },
			{ "pureblue", PUREBLUE }, { "puregreen", PUREGREEN }, { "crystal-clear", CRYSTAL_CLEAR },
		};

		bool readText(const std::string& path, std::string& out)
		{
			std::ifstream f(path, std::ios::binary);
			if (!f) return false;
			std::stringstream ss;
			ss << f.rdbuf();
			out = ss.str();
			return true;
		}
		bool readAddrs(const json::Value& v, std::map<std::string, Addr>& out, bool ram, std::string* why)
		{
			for (const auto& [k, a] : v.obj)
			{
				const std::string& s = a["addr"].str();
				char* end = nullptr;
				unsigned long n = s.size() > 2 ? strtoul(s.c_str(), &end, 16) : 0;
				int bank = (int)a["bank"].n;
				bool good = end && !*end && n <= 0xFFFF && bank >= 0 && a["bank"].type == json::Value::NUM;
				if (good && ram) good = ((n >= 0xC000 && n <= 0xDFFF) || (n >= 0xFF80 && n <= 0xFFFE)) &&
					(bank <= 1 || (bank <= 7 && n >= 0xD000 && n <= 0xDFFF));
				if (!good)
				{
					if (why) *why = (ram ? "ram." : "rom.") + k + " isn't an address it reads (" + s + ")";
					return false;
				}
				out[k] = Addr{ bank, (uint16_t)n };
			}
			return true;
		}

		// the games' header titles, from the data files there are (read once a folder; a copy, for any thread),
		// and whether a game code follows the title (the Game Boy Color games' releases have one)
		struct Title { std::string title, key; bool coded = false; };
		struct Index { std::string dir; std::vector<Title> titles; };
		Index index()
		{
			static std::mutex m;
			static Index idx;
			std::lock_guard<std::mutex> l(m);
			std::string dir = dataDir();
			if (idx.dir != dir || idx.titles.empty())
			{
				idx = Index{ dir, {} };
				for (const auto& g : GAMES)
				{
					GameData d;
					if (!loadData(g.key, d, nullptr)) continue;
					bool coded = false;
					for (const Release& r : d.releases) coded = coded || !r.code.empty();
					idx.titles.push_back({ d.headerTitle, g.key, coded });
				}
			}
			return idx;
		}
	}

	std::vector<uint8_t> readRomFile(const std::string& path, size_t limit)
	{
		std::vector<uint8_t> out;
		auto keep = [&](const uint8_t* p, size_t n) { out.insert(out.end(), p, p + std::min(n, limit - out.size())); };
		if (endsWithI(path, ".7z"))
		{
			if (ra::read7z(path, pickGame, MAX_ROM, keep) != ra::Unpack::OK) out.clear();
			return out;
		}
		if (endsWithI(path, ".zip"))
		{
			bool picked = false;
			bool read = readZip(path, [&](const std::vector<rnds::ZipMember>& ms) { int k = pickGame(ms); picked = k >= 0; return k; },
				[&](const uint8_t* p, size_t n) { keep(p, n); return out.size() < limit; });
			if (!picked || (!read && out.size() < limit)) out.clear();
			return out;
		}
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) return out;
		if (fseeko(f, 0, SEEK_END) == 0)
		{
			long long n = ftello(f);
			if (n > 0 && (uint64_t)n <= MAX_ROM) out.resize((size_t)std::min<long long>(n, (long long)limit));
		}
		if (fseeko(f, 0, SEEK_SET) != 0 || fread(out.data(), 1, out.size(), f) != out.size()) out.clear();
		fclose(f);
		return out;
	}

	std::string dataDir()
	{
		const char* e = getenv("KIWAMI_GBPK_DIR");
		return e && *e ? e : "/usr/share/banana/companions/gb-pokemon";
	}

	bool loadData(const std::string& key, GameData& out, std::string* why)
	{
		out = GameData{};
		const nds::GameId* id = nullptr;
		for (const auto& g : GAMES) if (key == g.key) id = &g.id;
		std::string path = dataDir() + "/" + key + ".ram.json", text;
		json::Value v;
		auto no = [&](const std::string& w) { if (why) *why = path + ": " + w; return false; };
		if (!id) return no("not one of the games");
		if (!readText(path, text)) return no("can't be read");
		if (!json::parse(text, v)) return no("isn't JSON");
		if (v["kind"].str() != "gb-pokemon-ram" || v["schema"].n != 1) return no("isn't a schema 1 gb-pokemon-ram file");
		if (v["game"].str() != key) return no("is " + v["game"].str() + "'s");
		out.key = key;
		out.gen = (int)v["gen"].n;
		if (out.gen != 1 && out.gen != 2) return no("has no generation 1 or 2");
		out.headerTitle = v["header_title"].str();
		if (out.headerTitle.empty() || out.headerTitle.size() > 16) return no("has no header title");
		for (const auto& r : v["releases"].arr)
		{
			Release rel{ r["sha1"].str(), r["name"].str(), r["code"].str(), {}, {} };
			if (rel.sha1.size() != 40) return no("has a release without its SHA-1");
			// (a release's own addresses: a ROM hack's versions that disagree)
			std::string w;
			if (!readAddrs(r["ram"], rel.ram, true, &w) || !readAddrs(r["rom"], rel.rom, false, &w))
				return no(w + " (release " + rel.name + ")");
			out.releases.push_back(rel);
		}
		if (out.releases.empty()) return no("has no releases");
		if (!readAddrs(v["ram"], out.ram, true, why) || !readAddrs(v["rom"], out.rom, false, why))
		{
			if (why) *why = path + ": " + *why;
			return false;
		}
		// (a release's own address is one its versions disagree on: every release has it, and the game doesn't, so
		// whichever is picked sets it)
		for (const Release& r : out.releases)
		{
			auto same = [](const std::map<std::string, Addr>& a, const std::map<std::string, Addr>& b) {
				return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto& x, const auto& y) { return x.first == y.first; });
			};
			bool good = same(r.ram, out.releases[0].ram) && same(r.rom, out.releases[0].rom);
			for (const auto& [k, a] : r.ram) good = good && !out.ram.count(k);
			for (const auto& [k, a] : r.rom) good = good && !out.rom.count(k);
			if (!good) return no("has a release whose own addresses aren't every release's and none of the game's (" + r.name + ")");
		}
		for (const auto& [k, n] : v["sizes"].obj) out.sizes[k] = (int)n.n;
		// the guide's flags: each bank at a ram key, whole bytes from a byte's first bit, none over another; the
		// derived bits after them, each over bits of the banks
		const int MAX_BITS = 8 * 4096;
		std::vector<bool> taken(MAX_BITS);
		for (const auto& f : v["flags"].arr)
		{
			FlagBank b{ f["ram"].str(), (int)f["bytes"].n, (int)f["first"].n };
			bool good = out.ram.count(b.ram) && b.bytes >= 1 && b.bytes <= 1024 && b.first >= 0 && b.first % 8 == 0 &&
				b.first + 8 * b.bytes <= MAX_BITS;
			for (int i = 0; good && i < 8 * b.bytes; i++) good = !taken[b.first + i];
			if (!good) return no("has a flag bank it can't lay out (" + b.ram + ")");
			for (int i = 0; i < 8 * b.bytes; i++) taken[b.first + i] = true;
			out.flags.push_back(b);
		}
		for (const auto& f : v["derived"].arr)
		{
			DerivedFlag dv{ (int)f["bit"].n, !f["all"].isNull(), {} };
			const json::Value& of = dv.all ? f["all"] : f["any"];
			bool good = dv.bit >= 0 && dv.bit < MAX_BITS && !taken[dv.bit] && of.size() >= 1 && f["all"].isNull() != f["any"].isNull();
			for (const auto& o : out.derived) good = good && o.bit != dv.bit;
			for (const auto& x : of.arr)
			{
				good = good && x.type == json::Value::NUM && x.n >= 0 && x.n < MAX_BITS && taken[(int)x.n];
				if (good) dv.of.push_back((int)x.n);
			}
			if (!good) return no("has a derived flag it can't work out (bit " + std::to_string(dv.bit) + ")");
			out.derived.push_back(dv);
		}
		out.game.id = *id;
		out.game.gen = out.gen == 1 ? GEN_I : GEN_II;
		out.game.title = v["title"].str();
		out.game.dexSize = out.gen == 1 ? 151 : 251;
		// the items ([id, Gen IV id] pairs), balls, key items, types ({ "id": "name" }) and split
		auto byte = [](const json::Value& x) { return x.type == json::Value::NUM && x.n >= 0 && x.n <= 255 && x.n == (int)x.n; };
		if (v["items"].type != json::Value::ARR || v["types"].type != json::Value::OBJ) return no("has no items or no types");
		for (const auto& e : v["items"].arr)
		{
			if (e.size() != 2 || !byte(e[0]) || e[0].n < 1 || e[1].type != json::Value::NUM || e[1].n < 1 || e[1].n > 65535)
				return no("has an item it can't read");
			out.items[(uint8_t)e[0].n] = (uint16_t)e[1].n;
		}
		auto ids = [&](const char* k, auto put) {
			for (const auto& e : v[k].arr) { if (!byte(e)) return false; put((uint8_t)e.n); }
			return v[k].isNull() || v[k].type == json::Value::ARR;
		};
		if (!ids("balls", [&](uint8_t i) { out.balls.insert(i); }) || !ids("key_items", [&](uint8_t i) { out.keyItems.insert(i); }) ||
			!ids("special", [&](uint8_t i) { out.special.set(i); }) || !ids("split_stats", [&](uint8_t i) { out.splitStats.set(i); }))
			return no("has a list of ids it can't read");
		for (const auto& [k, name] : v["types"].obj)
		{
			char* end = nullptr;
			const long t = strtol(k.c_str(), &end, 10);
			if (k.empty() || *end || t < 0 || t > 255) return no("has a type id it can't read (" + k + ")");
			out.types[(size_t)t] = typeByName(name.str());
			if (out.types[(size_t)t] == nds::NO_TYPE) return no("has a type it doesn't know (" + name.str() + ")");
		}
		out.romGuide = v["guide"].str() == "rom";
		const std::string cs = v["charset"].str();
		if (cs != "" && cs != "pure") return no("has a character set it doesn't know (" + cs + ")");
		out.pureText = cs == "pure";
		if (!v["dex_size"].isNull())
		{
			const json::Value& n = v["dex_size"];
			if (n.type != json::Value::NUM || n.n < 1 || n.n > 1023 || n.n != (int)n.n) return no("has a dex_size it can't use");
			out.game.dexSize = (int)n.n;
		}
		return true;
	}

	void pickRelease(GameData& d, int i)
	{
		d.release = i;
		if (i < 0 || i >= (int)d.releases.size()) return;
		for (const auto& [k, a] : d.releases[(size_t)i].ram) d.ram[k] = a;
		for (const auto& [k, a] : d.releases[(size_t)i].rom) d.rom[k] = a;
	}

	int captureRelease(const GameData& d, const std::string& readme)
	{
		for (size_t at = readme.find("sha1 "); at != std::string::npos; at = readme.find("sha1 ", at + 1))
		{
			size_t n = 0;
			while (at + 5 + n < readme.size() && isxdigit((unsigned char)readme[at + 5 + n])) n++;
			if (n < 8) continue;
			const std::string h = lower(readme.substr(at + 5, n));
			for (size_t i = 0; i < d.releases.size(); i++)
				if (d.releases[i].sha1.compare(0, h.size(), h) == 0) return (int)i;
		}
		return d.releases.empty() ? -1 : 0;
	}

	nds::Type typeByName(const std::string& name)
	{
		static const char* const NAMES[] = { "normal", "fighting", "flying", "poison", "ground", "rock", "bug", "ghost",
			"steel", "fire", "water", "grass", "electric", "psychic", "ice", "dragon", "dark" };	// (nds::Type's order)
		// (and the fork's own, from TYPELESS on)
		static const char* const FORK_NAMES[] = { "typeless", "crystal", "bonemerang", "tri", "floating", "magma" };
		if (const auto* f = std::find(std::begin(NAMES), std::end(NAMES), name); f != std::end(NAMES))
			return (nds::Type)(f - std::begin(NAMES));
		if (const auto* f = std::find(std::begin(FORK_NAMES), std::end(FORK_NAMES), name); f != std::end(FORK_NAMES))
			return (nds::Type)(TYPELESS + (f - std::begin(FORK_NAMES)));
		return nds::NO_TYPE;
	}

	std::vector<GameData> loadGames(const std::vector<std::string>& keys, std::string* why)
	{
		std::vector<GameData> out;
		for (const std::string& k : keys)
		{
			GameData d;
			std::string w;
			if (loadData(k, d, &w)) out.push_back(std::move(d));
			else if (why) *why += (why->empty() ? "" : "; ") + w;
		}
		return out;
	}

	std::string fixtureKey(const std::string& name)
	{
		std::string n = name, best;
		std::replace(n.begin(), n.end(), '_', '-');
		for (const auto& g : GAMES)
		{
			const size_t k = strlen(g.key);
			if (n.compare(0, k, g.key) == 0 && (n.size() == k || n[k] == '-') && k > best.size()) best = g.key;
		}
		return best;
	}

	Match detect(const uint8_t* header, size_t n, std::vector<std::string>& keys)
	{
		keys.clear();
		if (!header || n < 0x150) return Match::NONE;
		// the title at 0x134: 16 bytes in the first cartridges, 11 when a game code follows at 0x13F (the Color
		// games'); a shorter title ends with a 0
		const char* t = (const char*)header + 0x134;
		for (const auto& [title, k, coded] : index().titles)
			if (memcmp(t, title.data(), title.size()) == 0 && ((coded && title.size() >= 11) || title.size() >= 16 || t[title.size()] == 0))
				keys.push_back(k);
		return keys.empty() ? Match::NONE : Match::KNOWN;
	}

	namespace
	{
		// hash: the whole ROM's SHA-1 ("" for no ROM)
		Match confirmHash(const std::vector<uint8_t>& rom, const std::string& hash, GameData& d)
		{
			d.release = -1;
			d.game.romHash = hash;
			d.game.code.clear();
			for (size_t i = 0; i < d.releases.size(); i++)
				if (!rom.empty() && d.releases[i].sha1 == hash)
				{
					pickRelease(d, (int)i);
					// (the file's own code: the release's, since its SHA-1 is)
					if (rom.size() >= 0x143 && !d.releases[i].code.empty()) d.game.code.assign((const char*)rom.data() + 0x13F, 4);
					return Match::KNOWN;
				}
			return Match::UNSUPPORTED;
		}
		std::string sha1(const std::vector<uint8_t>& rom) { return rom.empty() ? "" : nds::nitro::sha1Hex(rom.data(), rom.size()); }
	}

	Match confirm(const std::vector<uint8_t>& rom, GameData& d)
	{
		return confirmHash(rom, sha1(rom), d);
	}

	int confirmAny(const std::vector<uint8_t>& rom, std::vector<GameData>& ds)
	{
		const std::string hash = sha1(rom);
		int picked = -1;
		for (size_t i = 0; i < ds.size(); i++)
			if (confirmHash(rom, hash, ds[i]) == Match::KNOWN && picked < 0) picked = (int)i;
		return picked;
	}
}
