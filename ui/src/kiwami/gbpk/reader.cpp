// banana-ui (kiwami): the Game Boy Pokémon companion's memory reader (reader.h). Generation II's structures as pret's
// pokecrystal and pokegold lay them out (macros/ram.asm: party_struct 48 bytes, battle_struct 32, roam_struct 7) and
// PokeDaisy's Gen2Reader.kt reads them: plaintext and big-endian, species by national number, DVs instead of IVs.
// Generation I's as pokered and pokeyellow lay them out (party_struct 44 bytes, battle_struct 29) and Gen1Reader.kt
// reads them: species by the game's internal index, one Special stat, no held items, one bag list, money in BCD.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "reader.h"
#include "rom.h"
#include "text.h"
#include "../../pokemon.h"
#include <algorithm>
#include <cstdio>

namespace banana::kiwami::gbpk
{
	namespace
	{
		const int PARTY = 6, MON = 48, BOX_MON = 32, BATTLE_MON = 32, NAME = 11, TMS_HMS = 57, FLAG_READ = 256;
		const int MON1 = 44, BATTLE_MON1 = 29, STAT_MODS1 = 6, DEX1 = 19;
		const uint8_t EGG = 0xFD, END = 0xFF;

		// Gen II's item ids and types in the pages' conventions (rom.h, shared with the ROM reader)
		uint16_t item(uint8_t id) { return gen2Item(id); }
		nds::Type type(uint8_t t) { return gen2Type(t); }
		uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

		// the memory a set's read reaches: reads in the same one are merged when they are close (the same WRAM bank,
		// HRAM, or the same bank from 0x10000)
		int region(uint32_t a) { return a >= 0x10000 ? 3 + (int)((a - 0x10000) >> 12) : a >= 0xFF80 ? 2 : a >= 0xD000 ? 1 : 0; }

		// the DVs (Attack << 4 | Defense, Speed << 4 | Special) into m: the IVs' places (HP's from the others' lowest
		// bits; Special as both special stats), its gender from the species' ratio, and its shininess through a pid
		// and an otSecret that make upstream's Mon::shiny() give the DVs' answer
		void setDVs(nds::Mon& m, uint8_t dv0, uint8_t dv1, const nds::RomData* rom)
		{
			const int atk = dv0 >> 4, def = dv0 & 15, spd = dv1 >> 4, spc = dv1 & 15;
			m.ivs[0] = (uint8_t)((atk & 1) << 3 | (def & 1) << 2 | (spd & 1) << 1 | (spc & 1));
			m.ivs[1] = (uint8_t)atk; m.ivs[2] = (uint8_t)def; m.ivs[3] = (uint8_t)spd; m.ivs[4] = m.ivs[5] = (uint8_t)spc;
			// (pret's engine/gfx/color.asm CheckShininess: Defense, Speed and Special 10, Attack's bit 1 set)
			const bool shiny = def == 10 && spd == 10 && spc == 10 && (atk & 2);
			m.pid = (uint32_t)dv0 << 24 | (uint32_t)dv1 << 16 | m.otId;
			const uint16_t hi = (uint16_t)(m.pid >> 16);
			m.otSecret = shiny ? hi : (uint16_t)(hi ^ 0xFFFF);
			// (pret's engine/pokemon/mon_stats.asm GetGender: female while Attack << 4 | Speed is at most the ratio;
			// 0 all male, 254 all female, 255 none)
			m.female = false;
			if (rom && m.species && m.species < rom->species.size() && !m.egg)
			{
				const int r = rom->species[m.species].genderRatio;
				m.female = r == 254 || (r != 0 && r != 255 && (atk << 4 | spd) <= r);
			}
		}

		// a box_struct (32 bytes) or party_struct (48) at p
		nds::Mon boxMon(const uint8_t* p, bool party, bool caughtData, bool egg, const nds::RomData* rom)
		{
			nds::Mon m;
			m.species = p[0];
			m.egg = egg;
			m.item = item(p[1]);
			for (int k = 0; k < 4; k++)
			{
				m.moves[k] = p[2 + k];			// (Gen II's moves are numbered as every later generation's)
				m.pp[k] = p[23 + k] & 0x3F;
				m.ppUps[k] = p[23 + k] >> 6;
			}
			m.otId = be16(p + 6);
			m.exp = (uint32_t)p[8] << 16 | (uint32_t)p[9] << 8 | p[10];
			for (int i = 0; i < 5; i++) m.statExp[i] = be16(p + 11 + 2 * i);
			m.friendship = p[27];
			m.pokerus = p[28];
			if (caughtData) m.metLevel = p[29] & 0x3F;	// (Crystal's; Gold and Silver leave the two bytes unused)
			m.level = p[31];
			setDVs(m, p[21], p[22], rom);
			if (party)
			{
				m.status = p[32];			// (Gen III's bits: sleep turns 0-2, PSN, BRN, FRZ, PAR)
				m.hp = be16(p + 34);
				m.maxHp = be16(p + 36);
				for (int i = 0; i < 5; i++) m.stats[i] = be16(p + 38 + 2 * i);	// Attack, Defense, Speed, Sp. Atk, Sp. Def
			}
			return m;
		}

		// a name as the pages write it: the game's own, or the species' when the Pokémon has none of its own
		void setNick(nds::Mon& m, const uint8_t* p, const nds::RomData* rom, int gen = 2)
		{
			m.nick = p ? decode(gen, p, NAME) : "";
			if (rom && m.species && m.species < rom->species.size())
			{
				const std::string& name = rom->species[m.species].name;
				std::string up = name;
				for (char& c : up) if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
				if (m.nick.empty() || m.nick == up) m.nick = name;
			}
		}

		// a battle_struct over m (the battler's own copy: its HP, status, stats, moves and types as the battle has them)
		nds::Battler battler(const uint8_t* p, const nds::Mon* base, const uint8_t* levels, const nds::RomData* rom)
		{
			nds::Battler b;
			nds::Mon& m = b.mon;
			if (base && base->species == p[0]) m = *base;
			else
			{
				m.species = p[0];
				setDVs(m, p[6], p[7], rom);
				m.friendship = p[12];
				setNick(m, nullptr, rom);
			}
			m.item = item(p[1]);
			for (int k = 0; k < 4; k++) { m.moves[k] = p[2 + k]; m.pp[k] = p[8 + k] & 0x3F; }
			m.level = p[13];
			m.status = p[14];
			m.hp = be16(p + 16);
			m.maxHp = be16(p + 18);
			for (int i = 0; i < 5; i++) m.stats[i] = be16(p + 20 + 2 * i);
			b.types[0] = type(p[30]);
			b.types[1] = type(p[31]);
			if (b.types[0] == nds::NO_TYPE) b.types[0] = b.types[1];
			if (b.types[1] == nds::NO_TYPE) b.types[1] = b.types[0];
			// the stat levels (pret: 7 is none, 1-13): Attack, Defense, Speed, Sp. Atk, Sp. Def, accuracy, evasion,
			// into the stages' places 1-7 (0 is HP's, unused)
			if (levels)
				for (int i = 0; i < 7; i++) b.stages[1 + i] = (int8_t)std::clamp(levels[i] - 7, -6, 6);
			b.present = m.species && m.hp > 0;
			return b;
		}

		// ---- Generation I ----

		// a species' national number by the game's internal index: the ROM's table (rom.h GbRomData::national), the
		// index as it is without one (no ROM read: the tests' stand-in); 0 for an index that isn't a species
		uint16_t national(uint8_t internal, const nds::RomData* rom)
		{
			const auto* gb = dynamic_cast<const GbRomData*>(rom);
			if (!gb || gb->national.empty()) return internal;
			return internal < gb->national.size() ? gb->national[internal] : 0;
		}

		// the DVs as Gen II's into the IVs' places (setDVs), with no gender and never shiny: the pid and otSecret only
		// tell one Pokémon from another
		void setDVs1(nds::Mon& m, uint8_t dv0, uint8_t dv1)
		{
			const int atk = dv0 >> 4, def = dv0 & 15, spd = dv1 >> 4, spc = dv1 & 15;
			m.ivs[0] = (uint8_t)((atk & 1) << 3 | (def & 1) << 2 | (spd & 1) << 1 | (spc & 1));
			m.ivs[1] = (uint8_t)atk; m.ivs[2] = (uint8_t)def; m.ivs[3] = (uint8_t)spd; m.ivs[4] = m.ivs[5] = (uint8_t)spc;
			m.pid = (uint32_t)dv0 << 24 | (uint32_t)dv1 << 16 | m.otId;
			m.otSecret = (uint16_t)((m.pid >> 16) ^ 0xFFFF);
			m.female = false;
		}

		// a party_struct (44 bytes) at p: the box_struct's species (internal), HP, status, types, catch rate, moves, OT
		// id, experience, stat experience, DVs and PP, then the level and the stats (Special once)
		nds::Mon partyMon1(const uint8_t* p, const nds::RomData* rom)
		{
			nds::Mon m;
			m.species = national(p[0], rom);
			m.hp = be16(p + 1);
			m.status = p[4];			// (Gen III's bits: sleep turns 0-2, PSN, BRN, FRZ, PAR)
			for (int k = 0; k < 4; k++)
			{
				m.moves[k] = p[8 + k];
				m.pp[k] = p[29 + k] & 0x3F;
				m.ppUps[k] = p[29 + k] >> 6;
			}
			m.otId = be16(p + 12);
			m.exp = (uint32_t)p[14] << 16 | (uint32_t)p[15] << 8 | p[16];
			for (int i = 0; i < 5; i++) m.statExp[i] = be16(p + 17 + 2 * i);
			setDVs1(m, p[27], p[28]);
			m.level = p[33];
			m.maxHp = be16(p + 34);
			for (int i = 0; i < 4; i++) m.stats[i] = be16(p + 36 + 2 * i);	// Attack, Defense, Speed, Special
			m.stats[4] = m.stats[3];			// (Special as both special stats, as the logic and pages read them)
			return m;
		}

		// a battle_struct (29 bytes) over the party member it is (its name, OT and stat experience kept): species, HP,
		// party position, status, the battle's types, catch rate, moves, DVs, level, the stats and PP; the stat mods
		// (pret: 7 is none, 1-13) Attack, Defense, Speed, Special, accuracy, evasion
		nds::Battler battler1(const uint8_t* p, const nds::Mon* base, const uint8_t* mods, const nds::RomData* rom)
		{
			nds::Battler b;
			nds::Mon& m = b.mon;
			const uint16_t species = national(p[0], rom);
			if (base && base->species == species) m = *base;
			else
			{
				m.species = species;
				setDVs1(m, p[12], p[13]);
				setNick(m, nullptr, rom, 1);
			}
			m.hp = be16(p + 1);
			m.status = p[4];
			for (int k = 0; k < 4; k++) { m.moves[k] = p[8 + k]; m.pp[k] = p[25 + k] & 0x3F; }
			m.level = p[14];
			m.maxHp = be16(p + 15);
			for (int i = 0; i < 4; i++) m.stats[i] = be16(p + 17 + 2 * i);
			m.stats[4] = m.stats[3];
			b.types[0] = type(p[5]);
			b.types[1] = type(p[6]);
			if (b.types[0] == nds::NO_TYPE) b.types[0] = b.types[1];
			if (b.types[1] == nds::NO_TYPE) b.types[1] = b.types[0];
			if (mods)
			{
				const int at[STAT_MODS1] = { 1, 2, 3, 4, 6, 7 };	// (the stages' places; Special's 4 also Sp. Def's 5)
				for (int i = 0; i < STAT_MODS1; i++) b.stages[at[i]] = (int8_t)std::clamp(mods[i] - 7, -6, 6);
				b.stages[5] = b.stages[4];
			}
			b.present = m.species && m.hp > 0;
			return b;
		}

		// three BCD bytes (wPlayerMoney: 99 99 99 is 999999)
		uint32_t bcd(const uint8_t* p)
		{
			uint32_t v = 0;
			for (int i = 0; i < 3; i++) v = v * 100 + (p[i] >> 4) * 10 + (p[i] & 15);
			return v;
		}
	}

	void Reader::open(const GameData& d)
	{
		close();
		d_ = d;
		open_ = true;
		// the set: each key with its length; the keys a data file lacks are left out
		const int items = d_.size("itemsMax", 20), balls = d_.size("ballsMax", 12), keys = d_.size("keyItemsMax", 25);
		const int bag = d_.size("bagSlots", 20);
		const std::vector<std::pair<const char*, int>> KEYS1 = {
			{ "partyCount", 1 + PARTY + 1 }, { "partyMons", PARTY * MON1 }, { "partyMonOTs", PARTY * NAME },
			{ "partyMonNicknames", PARTY * NAME }, { "playerID", 2 }, { "playerName", NAME }, { "money", 3 }, { "badges", 1 },
			{ "bagCount", 2 + 2 * bag }, { "curMap", 1 }, { "yCoord", 1 }, { "xCoord", 1 }, { "playTime", 5 },
			{ "dexCaught", DEX1 }, { "dexSeen", DEX1 }, { "isInBattle", 1 }, { "battleMon", BATTLE_MON1 },
			{ "enemyMon", BATTLE_MON1 }, { "playerStatMods", STAT_MODS1 }, { "enemyStatMods", STAT_MODS1 },
			{ "playerMonNumber", 1 }, { "trainerClass", 1 }, { "trainerNo", 1 }, { "enemyPartyCount", 1 + PARTY + 1 },
			{ "enemyMons", PARTY * MON1 }, { "enemyMonPartyPos", 1 }, { "pikachuHappiness", 1 },
		};
		const std::vector<std::pair<const char*, int>> KEYS2 = {
			{ "partyCount", 1 + PARTY + 1 }, { "partyMons", PARTY * MON }, { "partyMonOTs", PARTY * NAME },
			{ "partyMonNicknames", PARTY * NAME }, { "playerID", 2 }, { "playerName", NAME }, { "money", 3 },
			{ "johtoBadges", 1 }, { "kantoBadges", 1 }, { "tmsHms", TMS_HMS }, { "numItems", 2 + 2 * items },
			{ "numKeyItems", 2 + keys }, { "numBalls", 2 + 2 * balls }, { "mapGroup", 1 }, { "mapNumber", 1 },
			{ "yCoord", 1 }, { "xCoord", 1 }, { "gameTime", 5 }, { "timeOfDay", 1 }, { "dexCaught", 32 }, { "dexSeen", 32 },
			{ "battleMode", 1 }, { "battleMon", BATTLE_MON }, { "enemyMon", BATTLE_MON }, { "playerStatLevels", 7 },
			{ "enemyStatLevels", 7 }, { "curBattleMon", 1 }, { "otherTrainerClass", 1 }, { "otherTrainerID", 1 },
			{ "otPartyCount", 1 + PARTY + 1 }, { "otPartyMons", PARTY * MON }, { "curOTMon", 1 }, { "dayCareMan", 1 },
			{ "breedMon1", BOX_MON }, { "dayCareLady", 1 }, { "breedMon2", BOX_MON }, { "stepsToEgg", 1 },
			{ "roamMon1", 7 }, { "roamMon2", 7 }, { "roamMon3", 7 },
		};
		std::vector<Read> rs;
		for (const auto& [k, n] : d_.gen == 1 ? KEYS1 : KEYS2)
		{
			Addr a = d_.at(k);
			if (a.ok()) rs.push_back(Read{ a.core(), n, {}, false });
		}
		// the guide's flag banks, 256 bytes a read at most (before neighbours are merged)
		for (const FlagBank& f : d_.flags)
			for (int off = 0; off < f.bytes; off += FLAG_READ)
				rs.push_back(Read{ d_.at(f.ram).core() + (uint32_t)off, std::min(FLAG_READ, f.bytes - off), {}, false });
		std::sort(rs.begin(), rs.end(), [](const Read& a, const Read& b) { return a.addr < b.addr; });
		for (const Read& r : rs)
		{
			if (!plan_.empty())
			{
				Read& p = plan_.back();
				const uint32_t end = p.addr + (uint32_t)p.len;
				if (region(r.addr) == region(p.addr) && r.addr <= end + 32)
				{
					p.len = (int)std::max(end, r.addr + (uint32_t)r.len) - (int)p.addr;
					continue;
				}
			}
			plan_.push_back(r);
		}
	}

	void Reader::close()
	{
		*this = Reader();
	}

	std::string Reader::requests(double now)
	{
		if (!open_ || plan_.empty()) return "";
		if (now < nextAt_ || (waiting_ > 0 && now - sentAt_ < 10000)) return "";
		set_ = plan_;
		setErr_.clear();
		std::string out;
		char b[64];
		for (const Read& r : set_) { snprintf(b, sizeof b, "READ_CORE_MEMORY %x %d\n", r.addr, r.len); out += b; }
		waiting_ = (int)set_.size();
		sentAt_ = now;
		nextAt_ = now + 250;
		return out;
	}

	void Reader::feed(const char* data, size_t n)
	{
		part_.append(data, n);
		size_t nl;
		while ((nl = part_.find('\n')) != std::string::npos)
		{
			std::string ln = part_.substr(0, nl);
			part_.erase(0, nl + 1);
			uint32_t a;
			std::vector<uint8_t> v;
			if (!open_ || !pkmn::parseReply(ln, a, v)) continue;
			auto r = std::find_if(set_.begin(), set_.end(), [&](const Read& x) { return x.addr == a && !x.done; });
			if (r == set_.end()) continue;
			if (v.empty() && setErr_.empty())
			{
				// ("READ_CORE_MEMORY d892 -1 no memory map defined": the words after the -1)
				size_t at = ln.find("-1");
				setErr_ = at == std::string::npos ? "no data" : ln.substr(std::min(ln.size(), at + 3));
			}
			else if (!v.empty() && (int)v.size() != r->len && setErr_.empty()) setErr_ = "a reply shorter than asked";
			r->bytes = std::move(v);
			r->done = true;
			if (waiting_ > 0 && --waiting_ == 0) finish();
		}
		if (part_.size() > 65536) part_.clear();	// (a line with no end: not ours)
	}

	void Reader::finish()
	{
		if (!setErr_.empty()) { err_ = setErr_; return; }
		bool same = last_.size() == set_.size();
		for (size_t i = 0; same && i < set_.size(); i++) same = last_[i].addr == set_[i].addr && last_[i].bytes == set_[i].bytes;
		last_ = std::move(set_);
		set_.clear();
		err_.clear();
		if (!same) serial_++;
	}

	const uint8_t* Reader::at(const std::string& key, int off, int len) const
	{
		const Addr a = d_.at(key);
		if (!a.ok()) return nullptr;
		const uint32_t c = a.core() + (uint32_t)off;
		for (const Read& r : last_)
			if (c >= r.addr && c + (uint32_t)len <= r.addr + r.bytes.size()) return r.bytes.data() + (c - r.addr);
		return nullptr;
	}

	bool Reader::inBattle() const
	{
		const uint8_t* m = at(d_.gen == 1 ? "isInBattle" : "battleMode", 0, 1);
		return m && (*m == 1 || *m == 2);
	}

	// the guide's flags: each bank's bytes from its first bit, then the bits derived from them (none at all when a bank
	// wasn't read whole: a milestone isn't marked done by half a bank)
	void Reader::flagsInto(nds::Live& l) const
	{
		int bits = 0;
		for (const FlagBank& f : d_.flags) bits = std::max(bits, f.first + 8 * f.bytes);
		for (const DerivedFlag& f : d_.derived) bits = std::max(bits, f.bit + 1);
		std::vector<uint8_t> flags((size_t)(bits + 7) / 8);
		bool whole = true;
		for (const FlagBank& f : d_.flags)
			for (int off = 0; whole && off < f.bytes; off += FLAG_READ)
			{
				const int n = std::min(FLAG_READ, f.bytes - off);
				const uint8_t* p = at(f.ram, off, n);
				if (p) std::copy(p, p + n, flags.begin() + f.first / 8 + off);
				whole = p != nullptr;
			}
		auto bit = [&](int b) { return (flags[(size_t)b >> 3] >> (b & 7) & 1) != 0; };
		for (const DerivedFlag& f : d_.derived)
		{
			bool v = f.all;
			for (int b : f.of) v = f.all ? v && bit(b) : v || bit(b);
			if (v) flags[(size_t)f.bit >> 3] |= (uint8_t)(1 << (f.bit & 7));
		}
		if (whole) l.flags = std::move(flags);
	}

	std::shared_ptr<nds::Live> Reader::live(const nds::RomData* rom) const
	{
		if (d_.gen == 1) return live1(rom);
		auto l = std::make_shared<nds::Live>();
		// the party: a count of 1-6 whose species list (EGG for an egg) ends in 0xFF after it, and each struct's
		// species the list's (anything else is WRAM before a save is loaded)
		const uint8_t* cnt = at("partyCount", 0, 1 + PARTY + 1);
		const uint8_t* mons = at("partyMons", 0, PARTY * MON);
		if (!cnt || !mons) return l;
		const int n = cnt[0];
		if (n < 1 || n > PARTY || cnt[1 + n] != END) return l;
		const uint8_t* ots = at("partyMonOTs", 0, PARTY * NAME);
		const uint8_t* nicks = at("partyMonNicknames", 0, PARTY * NAME);
		const bool caughtData = d_.game.id == CRYSTAL;
		for (int i = 0; i < n; i++)
		{
			const uint8_t* p = mons + i * MON;
			const bool egg = cnt[1 + i] == EGG;
			if (!p[0] || (!egg && cnt[1 + i] != p[0]) || p[31] < 1 || p[31] > 100) return l;
			nds::Mon m = boxMon(p, true, caughtData, egg, rom);
			setNick(m, nicks ? nicks + i * NAME : nullptr, rom);
			if (ots) m.otName = decode(2, ots + i * NAME, NAME);
			l->party.push_back(m);
		}
		l->valid = true;

		if (const uint8_t* p = at("playerName", 0, NAME)) l->trainerName = decode(2, p, NAME);
		if (const uint8_t* p = at("playerID", 0, 2)) l->tid = be16(p);
		l->trainerKnown = at("playerID", 0, 2) != nullptr;
		const uint8_t *g = at("mapGroup", 0, 1), *num = at("mapNumber", 0, 1), *x = at("xCoord", 0, 1), *y = at("yCoord", 0, 1);
		if (g && num) l->zone = (uint16_t)(*g << 8 | *num);
		if (x) l->mapX = *x;
		if (y) l->mapY = *y;
		if (const uint8_t* p = at("money", 0, 3)) l->money = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
		if (const uint8_t* p = at("gameTime", 0, 5)) l->playSeconds = (uint32_t)be16(p) * 3600 + p[2] * 60 + p[3];
		const uint8_t *jb = at("johtoBadges", 0, 1), *kb = at("kantoBadges", 0, 1);
		l->badges = (uint16_t)((jb ? *jb : 0) | (kb ? *kb : 0) << 8);
		const uint8_t *seen = at("dexSeen", 0, 32), *caught = at("dexCaught", 0, 32);
		if (seen && caught)
		{
			// (a bit a species, bit n-1 for number n, as the pages read them)
			l->dexCaught.assign(caught, caught + 32);
			l->dexSeen.assign(seen, seen + 32);
			for (int i = 0; i < 32; i++) l->dexSeen[i] |= l->dexCaught[i];
		}
		flagsInto(*l);
		if (const uint8_t* t = at("timeOfDay", 0, 1))
			l->timeOfDay = *t == 0 ? nds::TimeOfDay::MORNING : *t == 1 ? nds::TimeOfDay::DAY : nds::TimeOfDay::NIGHT;

		// the bag: the items' and balls' lists ({item, count} pairs), the key items (ids), each ending in 0xFF after
		// its count, and the TM case (a count per TM01-50, HM01-07); before a save is loaded WRAM holds leftovers, so
		// no bag at all until every list is well formed (as PokeDaisy reads it)
		{
			std::map<uint8_t, std::vector<nds::BagItem>> bag;
			auto pairs = [&](const char* key, int max, nds::Pocket pocket)
			{
				const uint8_t* p = at(key, 0, 2 + 2 * max);
				if (!p || p[0] > max || p[1 + 2 * p[0]] != END) return false;
				for (int i = 0; i < p[0]; i++) if (p[1 + 2 * i]) bag[pocket].push_back({ item(p[1 + 2 * i]), p[2 + 2 * i] });
				return true;
			};
			const int keys = d_.size("keyItemsMax", 25);
			const uint8_t* k = at("numKeyItems", 0, 2 + keys);
			const uint8_t* tm = at("tmsHms", 0, TMS_HMS);
			bool ok = pairs("numItems", d_.size("itemsMax", 20), nds::POCKET_ITEMS) &&
				pairs("numBalls", d_.size("ballsMax", 12), nds::POCKET_BALLS) && k && k[0] <= keys && k[1 + k[0]] == END && tm;
			for (int i = 0; ok && i < TMS_HMS; i++) ok = tm[i] <= 99;
			if (ok)
			{
				for (int i = 0; i < k[0]; i++) if (k[1 + i]) bag[nds::POCKET_KEY].push_back({ item(k[1 + i]), 1 });
				// TM01-50 as Gen IV's TM01-50 (328-377), HM01-07 as its HM01-07 (420-426)
				for (int i = 0; i < TMS_HMS; i++)
					if (tm[i]) bag[nds::POCKET_TMS].push_back({ (uint16_t)(i < 50 ? 328 + i : 420 + i - 50), tm[i] });
				l->bag = std::move(bag);
			}
		}

		// the roamers (Raikou, Entei; Gold's and Silver's Suicune): one is out while its species is set and its map
		// group isn't -1 (caught or beaten: cleared)
		for (const char* key : { "roamMon1", "roamMon2", "roamMon3" })
			if (const uint8_t* r = at(key, 0, 7); r && r[0])
				l->roamers.push_back({ r[0], (uint16_t)(r[2] << 8 | r[3]), r[1], r[4], r[2] != 0xFF });
		// the day-care: the man's and the lady's (bit 0 of each: a Pokémon left with them)
		const uint8_t *man = at("dayCareMan", 0, 1), *lady = at("dayCareLady", 0, 1);
		if (man && (*man & 1)) if (const uint8_t* p = at("breedMon1", 0, BOX_MON)) l->daycare.push_back(boxMon(p, false, caughtData, false, rom));
		if (lady && (*lady & 1)) if (const uint8_t* p = at("breedMon2", 0, BOX_MON)) l->daycare.push_back(boxMon(p, false, caughtData, false, rom));
		if (const uint8_t* p = at("stepsToEgg", 0, 1)) l->eggSteps = *p;

		// the battle: wBattleMode 1 wild, 2 trainer
		const uint8_t* mode = at("battleMode", 0, 1);
		const uint8_t *bm = at("battleMon", 0, BATTLE_MON), *em = at("enemyMon", 0, BATTLE_MON);
		if (mode && (*mode == 1 || *mode == 2) && bm && em && bm[0] && em[0])
		{
			nds::Battle& b = l->battle;
			b.kind = *mode == 1 ? nds::BattleKind::WILD : nds::BattleKind::TRAINER;
			const uint8_t* cur = at("curBattleMon", 0, 1);
			const nds::Mon* ours = cur && *cur < l->party.size() ? &l->party[*cur] : nullptr;
			b.ours[0] = battler(bm, ours, at("playerStatLevels", 0, 7), rom);
			const nds::Mon* foe = nullptr;
			if (*mode == 2)
			{
				const uint8_t* oc = at("otPartyCount", 0, 1 + PARTY + 1);
				const uint8_t* om = at("otPartyMons", 0, PARTY * MON);
				if (oc && om && oc[0] <= PARTY)
					for (int i = 0; i < oc[0]; i++)
					{
						const uint8_t* p = om + i * MON;
						if (!p[0] || p[31] < 1 || p[31] > 100) break;
						nds::Mon m = boxMon(p, true, false, false, rom);
						setNick(m, nullptr, rom);
						b.foeParty.push_back(m);
					}
				const uint8_t* co = at("curOTMon", 0, 1);
				if (co && *co < b.foeParty.size()) foe = &b.foeParty[*co];
				const uint8_t *cls = at("otherTrainerClass", 0, 1), *id = at("otherTrainerID", 0, 1);
				if (cls && id) b.trainerId = (uint16_t)(*cls << 8 | *id);
			}
			b.foes[0] = battler(em, foe, at("enemyStatLevels", 0, 7), rom);
			b.turn = 1;		// (not read: the logic's turn-counting balls take the first turn)
		}
		return l;
	}

	// Generation I's set as the pages' Live (Red, Blue, Yellow)
	std::shared_ptr<nds::Live> Reader::live1(const nds::RomData* rom) const
	{
		auto l = std::make_shared<nds::Live>();
		// the party: as Gen II's, its species list the internal indexes the structs start with
		const uint8_t* cnt = at("partyCount", 0, 1 + PARTY + 1);
		const uint8_t* mons = at("partyMons", 0, PARTY * MON1);
		if (!cnt || !mons) return l;
		const int n = cnt[0];
		if (n < 1 || n > PARTY || cnt[1 + n] != END) return l;
		const uint8_t* ots = at("partyMonOTs", 0, PARTY * NAME);
		const uint8_t* nicks = at("partyMonNicknames", 0, PARTY * NAME);
		const uint8_t* name = at("playerName", 0, NAME);
		const uint8_t* id = at("playerID", 0, 2);
		// Yellow's Pikachu, the one the game's happiness is: the starter's species, the player's OT id and the OT
		// name's first five letters the player's (pokeyellow's IsThisPartyMonStarterPikachu)
		const uint8_t* happy = at("pikachuHappiness", 0, 1);
		const int starter = d_.size("starterPikachu", -1);
		std::vector<nds::Mon> party;
		for (int i = 0; i < n; i++)
		{
			const uint8_t* p = mons + i * MON1;
			if (!p[0] || cnt[1 + i] != p[0] || p[33] < 1 || p[33] > 100) return l;
			nds::Mon m = partyMon1(p, rom);
			if (!m.species) return l;	// (an index the ROM's table has no species for)
			setNick(m, nicks ? nicks + i * NAME : nullptr, rom, 1);
			if (ots) m.otName = decode(1, ots + i * NAME, NAME);
			if (happy && p[0] == starter && id && be16(p + 12) == be16(id) && ots && name &&
				std::equal(name, name + 5, ots + i * NAME))
				m.friendship = *happy;
			party.push_back(m);
		}
		l->party = std::move(party);
		l->valid = true;

		if (name) l->trainerName = decode(1, name, NAME);
		if (id) l->tid = be16(id);
		l->trainerKnown = id != nullptr;
		if (const uint8_t* p = at("curMap", 0, 1)) l->zone = *p;
		if (const uint8_t* p = at("xCoord", 0, 1)) l->mapX = *p;
		if (const uint8_t* p = at("yCoord", 0, 1)) l->mapY = *p;
		if (const uint8_t* p = at("money", 0, 3)) l->money = bcd(p);
		// (wPlayTimeHours, the maxed flag, minutes, seconds, frames)
		if (const uint8_t* p = at("playTime", 0, 5)) l->playSeconds = (uint32_t)p[0] * 3600 + p[2] * 60 + p[3];
		if (const uint8_t* p = at("badges", 0, 1)) l->badges = *p;
		const uint8_t *seen = at("dexSeen", 0, DEX1), *caught = at("dexCaught", 0, DEX1);
		if (seen && caught)
		{
			l->dexCaught.assign(caught, caught + DEX1);
			l->dexSeen.assign(seen, seen + DEX1);
			for (int i = 0; i < DEX1; i++) l->dexSeen[i] |= l->dexCaught[i];
		}
		flagsInto(*l);
		l->timeOfDay = nds::TimeOfDay::ANY;

		// the bag: one list of {item, count} pairs ending in 0xFF after its count, split into the pages' pockets
		{
			const int slots = d_.size("bagSlots", 20);
			const uint8_t* p = at("bagCount", 0, 2 + 2 * slots);
			if (p && p[0] <= slots && p[1 + 2 * p[0]] == END)
			{
				std::map<uint8_t, std::vector<nds::BagItem>> bag;
				for (int i = 0; i < p[0]; i++)
					if (p[1 + 2 * i]) bag[gen1Pocket(p[1 + 2 * i])].push_back({ gen1Item(p[1 + 2 * i]), p[2 + 2 * i] });
				l->bag = std::move(bag);
			}
		}

		// the battle: wIsInBattle 1 wild, 2 trainer (0xFF lost)
		const uint8_t* mode = at("isInBattle", 0, 1);
		const uint8_t *bm = at("battleMon", 0, BATTLE_MON1), *em = at("enemyMon", 0, BATTLE_MON1);
		if (mode && (*mode == 1 || *mode == 2) && bm && em && bm[0] && em[0])
		{
			nds::Battle& b = l->battle;
			b.kind = *mode == 1 ? nds::BattleKind::WILD : nds::BattleKind::TRAINER;
			const uint8_t* cur = at("playerMonNumber", 0, 1);
			const nds::Mon* ours = cur && *cur < l->party.size() ? &l->party[*cur] : nullptr;
			b.ours[0] = battler1(bm, ours, at("playerStatMods", 0, STAT_MODS1), rom);
			const nds::Mon* foe = nullptr;
			if (*mode == 2)
			{
				const uint8_t* oc = at("enemyPartyCount", 0, 1 + PARTY + 1);
				const uint8_t* om = at("enemyMons", 0, PARTY * MON1);
				if (oc && om && oc[0] <= PARTY)
					for (int i = 0; i < oc[0]; i++)
					{
						const uint8_t* p = om + i * MON1;
						if (!p[0] || p[33] < 1 || p[33] > 100) break;
						nds::Mon m = partyMon1(p, rom);
						if (!m.species) break;
						setNick(m, nullptr, rom, 1);
						b.foeParty.push_back(m);
					}
				const uint8_t* co = at("enemyMonPartyPos", 0, 1);
				if (co && *co < b.foeParty.size()) foe = &b.foeParty[*co];
				const uint8_t *cls = at("trainerClass", 0, 1), *no = at("trainerNo", 0, 1);
				if (cls && no) b.trainerId = (uint16_t)(*cls << 8 | *no);
			}
			b.foes[0] = battler1(em, foe, at("enemyStatMods", 0, STAT_MODS1), rom);
			b.turn = 1;
		}
		return l;
	}
}
