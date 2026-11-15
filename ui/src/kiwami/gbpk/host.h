#pragma once
// banana-ui (kiwami): the data of the Game Boy Pokémon companion's pages, as upstream's gba::Host makes the GBA
// companion's: the player's ROM read and checked once on a thread of its own (its SHA-1 among the releases of the
// games its header names, which picks one, then its tables: rom.h), a Session on its own thread fed with each new set of the game's memory as an
// nds::Live (reader.h: the journey log, the advice, the notices), and the PageData the game panel's pages draw
// (panel.h). The memory reader is opened only once the ROM is confirmed: a release the companion doesn't know ("Not
// supported") is never asked anything. While a Game Boy game's data is in use its rules are upstream's logic's
// (rules.h useRules, and useGameRules with its ROM's type chart), and put back when the host goes. The guide is the game's file in the data folder (Crystal's),
// with the ROM's hidden items. PureRGB's save switches some of its rules: the matchup options and the signature moves
// go to the rules with each set, and the species given back their original types make the Session start again on a
// copy of the ROM's data with those types; Crystal Clear's trainers' parties go by the save's badges the same way.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include "reader.h"
#include "rules.h"
#include "../../nds/model.h"
#include "../../nds/pages.h"
#include "../../nds/session.h"
#include <future>
#include <memory>
#include <string>

namespace banana::kiwami::gbpk
{
	class Host
	{
	public:
		// ds: the games the header named (their data files, the retail game first; one for a stand-in), the ROM's
		// SHA-1 picks the host's; rom: the file the game was started from; reader: opened here once the ROM is
		// confirmed, closed when the host goes. stateDir: where the journey log is kept on the card
		// (cache/gb-companion); challenge: Settings' Challenge mode row (gba_challenge). standIn (tests): the game's
		// data without a ROM to read: no check, no tables but these
		Host(std::vector<GameData> ds, std::string rom, Reader& reader, std::string stateDir, bool challenge,
			std::shared_ptr<const nds::RomData> standIn = nullptr);
		~Host();
		// each turn of the menu's loop: the ROM's data picked up, a new set of the game's memory handed to the Session,
		// the Session's newest taken. True when the pages have new data (takeData): from the first, "Reading your game"
		// till the ROM's data is read
		bool tick(double nowMs);
		bool ready() const { return base.rom != nullptr; }		// the ROM's data read
		bool failed() const { return loadFailed; }				// it couldn't be (the pages say "No game data")
		bool unsupported() const { return loadFailed && base.unsupported; }	// a release it doesn't know: "Not supported"
		bool takeData(nds::PageData& out);
		void markSeen(const std::vector<std::string>& ids);	// what the pages saw (Pages::takeSeen)
		std::string title() const { return data.game.title; }	// the picked game's (till then the first's)
		bool inBattle() const { return reader.active() && reader.inBattle(); }
		uint64_t fed = 0;		// (statistics: states handed to the logic)
	private:
		struct Loaded { std::shared_ptr<const nds::RomData> rom; GameData d; bool unsupported = false;
			std::shared_ptr<const nds::GuideData> guide; };
		std::vector<GameData> games;
		GameData data;			// the picked game (till then the first)
		std::string romPath, stateDir;
		Reader& reader;
		bool challenge;
		std::shared_ptr<const nds::RomData> standIn;
		std::future<Loaded> load;
		bool started = false, loading = false, loadFailed = false;
		nds::PageData base, out;
		bool fresh = false;
		std::unique_ptr<nds::SessionThread> session;
		std::unique_ptr<GameRules> rules;	// the game's own (its ROM's): rules.h gameRules while it's set
		uint64_t lastSerial = 0;
		// PureRGB's switches in the save: the ROM's data as read (base.rom is a copy of it with the switched species'
		// original types, rom.h withOriginalTypes), and the last set's switches
		std::shared_ptr<const nds::RomData> asRead;
		bool pure = false, switchesSeen = false, lastSigOff = false;
		uint32_t lastRemap = 0;
		uint8_t lastOptions = 0;
		// Crystal Clear's trainers whose party goes by the badge count (rom.h withBadgeTeams): the party base.rom has
		bool teams = false;
		int lastTeam = 0;
		void begin(Loaded l);
		void switches();
		void badgeTeams(uint16_t badges);
		void startSession();
	};
}
