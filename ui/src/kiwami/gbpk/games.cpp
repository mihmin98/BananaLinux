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

		const struct { const char* key; nds::GameId id; } GAMES[] = {
			{ "red", RED }, { "blue", BLUE }, { "yellow", YELLOW }, { "gold", GOLD }, { "silver", SILVER }, { "crystal", CRYSTAL },
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

		// the six games' header titles, from the data files there are (read once a folder; a copy, for any thread),
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
		if (!id) return no("not one of the six games");
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
			Release rel{ r["sha1"].str(), r["name"].str(), r["code"].str() };
			if (rel.sha1.size() != 40) return no("has a release without its SHA-1");
			out.releases.push_back(rel);
		}
		if (out.releases.empty()) return no("has no releases");
		if (!readAddrs(v["ram"], out.ram, true, why) || !readAddrs(v["rom"], out.rom, false, why))
		{
			if (why) *why = path + ": " + *why;
			return false;
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
		return true;
	}

	Match detect(const uint8_t* header, size_t n, std::string& key)
	{
		key.clear();
		if (!header || n < 0x150) return Match::NONE;
		// the title at 0x134: 16 bytes in the first cartridges, 11 when a game code follows at 0x13F (the Color
		// games'); a shorter title ends with a 0
		const char* t = (const char*)header + 0x134;
		for (const auto& [title, k, coded] : index().titles)
			if (memcmp(t, title.data(), title.size()) == 0 && ((coded && title.size() >= 11) || title.size() >= 16 || t[title.size()] == 0))
			{
				key = k;
				return Match::KNOWN;
			}
		return Match::NONE;
	}

	Match confirm(const std::vector<uint8_t>& rom, GameData& d)
	{
		d.release = -1;
		d.game.romHash = rom.empty() ? "" : nds::nitro::sha1Hex(rom.data(), rom.size());
		d.game.code.clear();
		for (size_t i = 0; i < d.releases.size(); i++)
			if (!rom.empty() && d.releases[i].sha1 == d.game.romHash)
			{
				d.release = (int)i;
				// (the file's own code: the release's, since its SHA-1 is)
				if (rom.size() >= 0x143 && !d.releases[i].code.empty()) d.game.code.assign((const char*)rom.data() + 0x13F, 4);
				return Match::KNOWN;
			}
		return Match::UNSUPPORTED;
	}
}
