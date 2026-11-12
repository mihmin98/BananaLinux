// banana-ui (kiwami): the Game Boy Pokémon companion for the game running (game.h).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "game.h"
#include "panel.h"
#include "../../settings.h"
#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// a key's value in a settings file (key = value, the value maybe quoted); "" when it isn't there
		std::string confValue(const std::string& file, const std::string& key)
		{
			std::ifstream f(file);
			for (std::string line; std::getline(f, line);)
			{
				const size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				auto trim = [](std::string s) {
					const char* ws = " \t\r\"";
					s.erase(0, s.find_first_not_of(ws));
					s.erase(s.find_last_not_of(ws) + 1);
					return s;
				};
				if (trim(line.substr(0, eq)) == key) return trim(line.substr(eq + 1));
			}
			return "";
		}
	}

	void Game::open(bool panel, const std::string& system, const std::string& rom, const std::string& settingsConf, bool taken,
		const char* outPath, int& outFd)
	{
		close();
		if (!panel || (system != "gb" && system != "gbc") || taken) return;
		if (confValue(settingsConf, "kiwami_pokemon_companion_" + system) != "on") return;
		const std::vector<uint8_t> header = readRomFile(rom, 0x150);
		std::string key, why;
		if (detect(header.data(), header.size(), key) != Match::KNOWN) return;
		GameData d;
		if (!loadData(key, d, &why)) { fprintf(stderr, "banana-ui: gb companion: no data file: %s\n", why.c_str()); return; }
		// RetroArch's stdout, held open both ways as upstream holds it
		if (outFd < 0)
		{
			unlink(outPath);
			if (mkfifo(outPath, 0600) == 0) outFd = ::open(outPath, O_RDWR | O_NONBLOCK | O_CLOEXEC);
			if (outFd < 0) { fprintf(stderr, "banana-ui: gb companion: no pipe for RetroArch's replies\n"); return; }
		}
		known = true;
		data = d;
		romPath = rom;
		fprintf(stderr, "banana-ui: gb companion: %s\n", data.game.title.c_str());
	}

	void Game::attach(int outFd, const std::string& dataDir)
	{
		if (!known || outFd < 0) return;
		mkdir((dataDir + "/cache").c_str(), 0755);
		mkdir((dataDir + "/cache/gb-companion").c_str(), 0755);
		host = std::make_unique<Host>(data, romPath, reader, dataDir + "/cache/gb-companion",
			confValue(dataDir + "/settings.conf", "gba_challenge") == "on", standIn);	// (upstream's Challenge mode row)
		PanelSwitch::pending(host.get());
	}

	std::string Game::requests(double now)
	{
		return known && reader.active() ? reader.requests(now) : std::string();
	}

	bool Game::feed(const char* d, size_t n)
	{
		if (!known) return false;
		if (reader.active()) reader.feed(d, n);	// (before the ROM is confirmed nothing was asked: nothing to read)
		return true;
	}

	void Game::release(Settings& view, std::mutex& uiLock)
	{
		if (!host) return;
		{
			std::lock_guard<std::mutex> l(uiLock);
			view.gbaRelease();
		}
		const uint64_t fed = host->fed;
		host.reset();
		fprintf(stderr, "banana-ui: gb companion: %llu states handed to the logic\n", (unsigned long long)fed);
	}

	void Game::close()
	{
		PanelSwitch::pending(nullptr);
		host.reset();
		reader.close();
		known = false;
		data = GameData();
		romPath.clear();
	}

	// ---- the headless tests --------------------------------------------------------------------------------------

	namespace
	{
		// a stand-in for the ROM's tables, for a fixture's game with no ROM (CI has none): every species, move and
		// item named by its number, nothing else (Gen I's species by their internal index, all 255 of them: there is no
		// table to give their national numbers)
		struct StandIn : nds::RomData
		{
			std::shared_ptr<const nds::Sprite> sprite(uint16_t, bool, uint8_t) const override { return nullptr; }
		};
		std::shared_ptr<const nds::RomData> makeStandIn(const GameData& d)
		{
			auto r = std::make_shared<StandIn>();
			r->game = d.game;
			r->species.resize(d.gen == 1 ? 256 : (size_t)d.game.dexSize + 1);
			for (size_t s = 1; s < r->species.size(); s++)
			{
				nds::Species& sp = r->species[s];
				sp.name = "Species " + std::to_string(s);
				std::fill(std::begin(sp.base), std::end(sp.base), 50);
				sp.catchRate = 45;
				sp.genderRatio = d.gen == 1 ? 255 : 127;	// (no genders in Gen I: 255, none, draws no sign)
			}
			r->moves.resize(252);
			for (size_t m = 1; m < r->moves.size(); m++) { r->moves[m].name = "Move " + std::to_string(m); r->moves[m].power = 40; r->moves[m].pp = 35; r->moves[m].accuracy = 100; }
			r->items.resize(1256);
			for (size_t i = 1; i < r->items.size(); i++) r->items[i].name = "Item " + std::to_string(i);
			r->tmMoves.assign(100, 0);
			return r;
		}

		// a capture's memory (kiwami/tests/gbpk/fixtures/<name>: wram.bin C000-DFFF, hram.bin FF80-FFFE, wramx.bin the
		// eight WRAM banks, none for a Game Boy game's) as READ_CORE_MEMORY reads it: C000-CFFF, D000-DFFF bank 1 (or
		// wram.bin's without banks), HRAM, banks 2-7 from 0x10000
		struct Capture
		{
			std::vector<uint8_t> wram, hram, wramx;
			bool load(const std::string& dir)
			{
				auto file = [&](const char* f, std::vector<uint8_t>& o) {
					std::ifstream in(dir + "/" + f, std::ios::binary);
					o.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				};
				file("wram.bin", wram); file("hram.bin", hram); file("wramx.bin", wramx);
				return wram.size() == 0x2000 && (wramx.empty() || wramx.size() == 0x8000) && hram.size() >= 0x7F;
			}
			int at(uint32_t a) const
			{
				if (a >= 0xC000 && a < 0xD000) return wram[a - 0xC000];
				if (a >= 0xD000 && a < 0xE000) return wramx.empty() ? wram[a - 0xC000] : wramx[0x1000 + a - 0xD000];
				if (a >= 0xFF80 && a < 0xFFFF) return hram[a - 0xFF80];
				if (a >= 0x10000 && a < 0x16000 && !wramx.empty()) return wramx[0x2000 + a - 0x10000];
				return -1;
			}
		};
		// the replies to a set of requests (a read outside the capture: the emulator's error, as RetroArch answers)
		std::string answer(const std::string& req, const Capture& c)
		{
			std::string out;
			std::istringstream in(req);
			for (std::string line; std::getline(in, line);)
			{
				unsigned a = 0; int n = 0;
				if (sscanf(line.c_str(), "READ_CORE_MEMORY %x %d", &a, &n) != 2) continue;
				char b[40];
				snprintf(b, sizeof b, "READ_CORE_MEMORY %x", a);
				std::string l = b;
				for (int i = 0; i < n; i++)
				{
					const int v = c.at(a + (uint32_t)i);
					if (v < 0) { l = std::string(b) + " -1 no memory map defined"; break; }
					snprintf(b, sizeof b, " %02x", v);
					l += b;
				}
				out += l + "\n";
			}
			return out;
		}
	}

	bool Game::script(const std::string& op, std::istream& cs, Settings& s, const std::string& out, double now)
	{
		static Game g;
		if (op == "gbpanel")
		{
			int scr = 1;
			std::string dir, rom;
			cs >> scr >> dir;
			std::getline(cs >> std::ws, rom);
			s.gbaRelease();
			g.close();
			// the game: the ROM's header when one is given, else the fixture's folder name (crystal_battle: crystal)
			std::string key = dir.substr(dir.find_last_of('/') + 1), why;
			key = key.substr(0, key.find('_'));
			bool ok = true;
			if (!rom.empty())
			{
				const std::vector<uint8_t> h = readRomFile(rom, 0x150);
				ok = detect(h.data(), h.size(), key) == Match::KNOWN;
			}
			if (ok && !loadData(key, g.data, &why)) { fprintf(stderr, "banana-ui: gbpanel: %s\n", why.c_str()); ok = false; }
			if (ok)
			{
				g.known = true;
				g.romPath = rom;
				g.standIn = rom.empty() ? makeStandIn(g.data) : nullptr;
				mkdir((out + "gb-state").c_str(), 0755);
				g.host = std::make_unique<Host>(g.data, rom, g.reader, out + "gb-state", false, g.standIn);
				PanelSwitch::pending(g.host.get());
			}
			s.gameOpen("Pokemon Test", "Game Boy Color", scr, true);
			s.gbaAttach(nullptr);
			fprintf(stderr, "banana-ui: gbpanel %s%s%s: companion %s\n", dir.c_str(), rom.empty() ? "" : " ", rom.c_str(),
				ok ? g.data.game.title.c_str() : "none");
			return true;
		}
		if (op == "gbfeed")
		{
			std::string dir;
			std::getline(cs >> std::ws, dir);
			Capture c;
			if (!c.load(dir)) { fprintf(stderr, "banana-ui: gbfeed %s: no capture\n", dir.c_str()); return true; }
			const std::string r = g.requests(now);
			const std::string a = answer(r, c);
			if (!a.empty()) g.feed(a.data(), a.size());
			s.gameUpdate(now);
			auto l = g.reader.active() ? g.reader.live(nullptr) : nullptr;
			fprintf(stderr, "banana-ui: gbfeed %s: asked %d, party %zu, battle %d, view %s\n", dir.c_str(),
				(int)std::count(r.begin(), r.end(), '\n'), l && l->valid ? l->party.size() : (size_t)0,
				g.reader.active() && g.reader.inBattle() ? 1 : 0, s.gamePanelView().c_str());
			return true;
		}
		if (op == "gbhost")
		{
			for (int i = 0; i < 1000; i++)
			{
				s.gameUpdate(now);
				if (s.gbaReady() != 0) break;
				usleep(10000);
			}
			s.gameUpdate(now);
			const int r = s.gbaReady();
			fprintf(stderr, "banana-ui: gbhost: %s %s view %s\n", r > 0 ? "ready" : r < -1 ? "unsupported" : r < 0 ? "failed" : "timeout",
				s.gbaTitle().c_str(), s.gamePanelView().c_str());
			return true;
		}
		if (op == "gbdrag")
		{
			float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
			cs >> x1 >> y1 >> x2 >> y2;
			s.gameTouch(x1, y1, true);
			s.gameTouchMove(x2, y2);
			s.gameTouch(x2, y2, false);
			s.gameUpdate(now);
			fprintf(stderr, "banana-ui: gbdrag %.0f %.0f %.0f %.0f: view %s\n", x1, y1, x2, y2, s.gamePanelView().c_str());
			return true;
		}
		return false;
	}
}
