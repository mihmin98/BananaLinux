#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's memory reader. While a game plays it asks the emulator for the
// game's WRAM through READ_CORE_MEMORY (RetroArch's stdin, or BananaFront's shared memory: the game panel sends what
// requests() gives and feeds the replies back), the way upstream's pkmn::Companion does for the GBA: a set of reads
// every 250 ms, the next only once the last was answered, asked again after 10 s without an answer. A set's replies
// make a snapshot of the game's memory, and live() turns it into upstream's nds::Live, in the DS companion's
// conventions (species by national number, items by Gen IV number, TMs as Gen IV numbers them), so upstream's
// session, logic and pages take it.
// Generation II for now (Gold, Silver, Crystal); the addresses are the data file's (games.h), with the flag banks its
// guide reads (Live::flags).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include <memory>
#include <string>
#include <vector>

namespace banana::kiwami::gbpk
{
	class Reader
	{
	public:
		// the game to read (a confirmed one's data); nothing is asked before
		void open(const GameData& d);
		void close();
		bool active() const { return open_; }
		const GameData& data() const { return d_; }

		// the READ_CORE_MEMORY lines to send now ("" none)
		std::string requests(double nowMs);
		// the emulator's output (whole lines or pieces; lines that aren't READ_CORE_MEMORY replies are skipped)
		void feed(const char* data, size_t n);

		// counts the sets whose memory differed from the one before
		uint64_t serial() const { return serial_; }
		// the last set: a battle on (wild or trainer)
		bool inBattle() const;
		// the emulator's reply when it had no data ("no memory map defined"), from the last set that had one; cleared by
		// a set answered in full
		const std::string& error() const { return err_; }
		// the last complete set as the pages' live state; rom (may be null) gives the species' gender ratios and names.
		// valid is false until a set with a party of 1-6 whose species list matches it was read.
		std::shared_ptr<nds::Live> live(const nds::RomData* rom) const;

	private:
		struct Read { uint32_t addr = 0; int len = 0; std::vector<uint8_t> bytes; bool done = false; };
		GameData d_;
		bool open_ = false;
		std::vector<Read> plan_;		// the reads of a set (the data file's addresses, neighbours merged)
		std::vector<Read> set_, last_;	// the set being answered, the last answered in full
		int waiting_ = 0;
		double nextAt_ = 0, sentAt_ = 0;
		std::string part_, err_, setErr_;
		uint64_t serial_ = 0;
		void finish();
		// the last set's bytes at a key's address (+off), len of them; null when the key or the bytes aren't there
		const uint8_t* at(const std::string& key, int off, int len) const;
	};
}
