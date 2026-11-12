#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion for the game running, what main.cpp holds (its kiwami: hooks):
// at a game's launch, whether the companion opens (Settings > Other systems > Second screen's row of the system the
// game was started from, a game panel, no other companion for the game, and a header title of the six games) and the
// pipe for RetroArch's replies; while it plays, the reader's requests and the replies; the host for the panel's pages
// (host.h), handed to Settings' panel (panel.h); and the headless test verbs gbpanel, gbfeed, gbhost and gbdrag.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include "host.h"
#include "reader.h"
#include <istream>
#include <memory>
#include <mutex>
#include <string>

namespace banana { class Settings; }

namespace banana::kiwami::gbpk
{
	class Game
	{
	public:
		// a game starts: the companion when panel (the game panel is on), system is gb or gbc, its row
		// (kiwami_pokemon_companion_<system> in settingsConf) is on, taken is false (upstream's Pokémon or game companion
		// has the game) and the ROM's header names one of the six. Then the reply pipe at outPath, when outFd has none
		// (-1), as upstream makes it. The last game's is closed first.
		void open(bool panel, const std::string& system, const std::string& rom, const std::string& settingsConf, bool taken,
			const char* outPath, int& outFd);
		// the game panel opens (outFd: the reply pipe): the host, handed to Settings' panel at its gbaAttach. data: the
		// data folder (the journey log in cache/gb-companion, settings.conf's gba_challenge)
		void attach(int outFd, const std::string& data);
		bool active() const { return known; }
		// the reader's requests ("" until the ROM is confirmed), and RetroArch's replies: false when not active (the
		// replies are upstream's)
		std::string requests(double nowMs);
		bool feed(const char* data, size_t n);
		// the game ended: the panel lets go of the host under the lock, then the host goes (the Session saves the journey)
		void release(Settings& view, std::mutex& uiLock);
		void close();

		// the headless script's verbs (true: op was one of them):
		//   gbpanel <screen> <fixture folder> [rom]: the game panel with the companion for a fixture's game (its folder's
		//     name: crystal_battle is Crystal), with a stand-in RomData, or the ROM given read and checked
		//   gbfeed <fixture folder>: the reader's requests answered from that capture, then the panel updated
		//   gbhost: waits (up to 10 s) for the ROM's data, as upstream's gbahost does for the GBA's
		//   gbdrag <x1> <y1> <x2> <y2>: a finger down at one point, moved to the other and lifted, on the game panel, in
		//     the panel's own px (upstream's gtouch only taps, in px of a 640x480 panel)
		static bool script(const std::string& op, std::istream& args, Settings& s, const std::string& out, double now);

	private:
		bool known = false;
		GameData data;
		std::string romPath;
		Reader reader;
		std::unique_ptr<Host> host;
		std::shared_ptr<const nds::RomData> standIn;	// (tests)
	};
}
