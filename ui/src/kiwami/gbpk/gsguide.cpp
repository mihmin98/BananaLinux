// banana-ui (kiwami): Gold's, Silver's and Crystal Clear's guide from the ROM's trainers, and a guide file's levels from
// them (gsguide.h).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "gsguide.h"
#include "rom.h"
#include <algorithm>
#include <bitset>
#include <cctype>
#include <map>
#include <set>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// which badge each leader's gym gives: the gym's script's setflag ENGINE_<badge> (pret's pokegold maps/*Gym.asm,
		// Clair's maps/DragonsDenB1F.asm). The class ids and the badges' bits are the data file's, from pret's
		// constants (kiwami/tools/gbpk-symbols.txt; Crystal Clear's classes ported, gbpk-port.py); a Kanto badge is a
		// bit of wKantoBadges, Live::badges' high byte
		struct Leader { const char* cls; const char* badge; bool kanto; };
		constexpr Leader LEADERS[] = {
			{ "leaderFalkner", "badgeZephyr", false }, { "leaderBugsy", "badgeHive", false },
			{ "leaderWhitney", "badgePlain", false }, { "leaderMorty", "badgeFog", false },
			{ "leaderJasmine", "badgeMineral", false }, { "leaderChuck", "badgeStorm", false },
			{ "leaderPryce", "badgeGlacier", false }, { "leaderClair", "badgeRising", false },
			{ "leaderBrock", "badgeBoulder", true }, { "leaderMisty", "badgeCascade", true },
			{ "leaderLtSurge", "badgeThunder", true }, { "leaderErika", "badgeRainbow", true },
			{ "leaderJanine", "badgeSoul", true }, { "leaderSabrina", "badgeMarsh", true },
			{ "leaderBlaine", "badgeVolcano", true }, { "leaderBlue", "badgeEarth", true },
		};

		std::string lower(const std::string& s)
		{
			std::string o;
			for (char c : s) o += (char)std::tolower((unsigned char)c);
			return o;
		}
		// "Lt. Surge" -> "lt-surge", as crystal.json's ids are
		std::string slug(const std::string& s)
		{
			std::string o;
			for (char c : s)
				if (std::isalnum((unsigned char)c)) o += (char)std::tolower((unsigned char)c);
				else if (!o.empty() && o.back() != '-') o += '-';
			while (!o.empty() && o.back() == '-') o.pop_back();
			return o;
		}
	}

	nds::GuideData romGuide(const nds::RomData& rom, const GameData& d, uint16_t badges)
	{
		nds::GuideData g;
		g.game = d.key;
		g.title = "Pokémon " + d.game.title;
		g.sources = { d.guideRegions
			? "The player's ROM: its gym leaders, Elite Four and Champion from the trainer table, each class found by its "
			  "name and its first trainer's as pret/pokecrystal's, each leader's badge by its constants (kiwami/GB-POKEMON.md)"
			: "The player's ROM: its gym leaders, Elite Four and Champion from the trainer table, each leader's badge "
			  "by pret/pokegold's constants (kiwami/GB-POKEMON.md)" };

		// the leader classes' badges: 1-8 Johto's, 9-16 Kanto's (done_when badge:N)
		std::map<int, int> badgeOf;
		for (const Leader& l : LEADERS)
		{
			const int cls = d.size(l.cls, -1), bit = d.size(l.badge, -1);
			if (cls > 0 && bit >= 0 && bit < 8) badgeOf[cls] = (l.kanto ? 8 : 0) + bit + 1;
		}

		// the Elite Four's and the Champion's classes
		std::set<int> eliteFour;
		for (const char* k : { "eliteWill", "eliteBruno", "eliteKaren", "eliteKoga" })
			if (const int cls = d.size(k, -1); cls > 0) eliteFour.insert(cls);
		const int champion = d.size("champion", -1);

		// the stages, in the story's order: Johto's leaders first, or in regions' order the save's region's (Kanto's
		// when it has more of Kanto's badges than of Johto's)
		enum { FIRST, ELITE, CHAMPION, SECOND };
		const bool kantoFirst = d.guideRegions && std::bitset<8>(badges >> 8).count() > std::bitset<8>(badges).count();
		struct Foe { int stage = FIRST, top = 999, cls = 0, badge = 0; std::vector<uint16_t> ids; const nds::Trainer* t = nullptr; };
		std::map<int, Foe> foes;	// by class
		for (const auto& kv : rom.trainers)
		{
			const nds::Trainer& t = kv.second;
			const int cls = t.id >> 8;
			int stage, badge = 0;
			if (lower(t.cls) == "leader")
			{
				auto b = badgeOf.find(cls);
				if (b != badgeOf.end()) badge = b->second;
				stage = (badge > 8) == kantoFirst ? FIRST : SECOND;
			}
			else if (eliteFour.count(cls)) stage = ELITE;
			else if (cls == champion) stage = CHAMPION;
			else continue;
			if (t.name.empty() || t.team.empty()) continue;
			int top = 0;
			for (const nds::TrainerMon& m : t.team) top = std::max(top, (int)m.level);
			Foe& f = foes[cls];
			f.stage = stage;
			f.cls = cls;
			f.badge = badge;
			f.ids.push_back(t.id);
			if (top < f.top) { f.top = top; f.t = &t; }	// (the first fight: its lowest-levelled team)
		}
		std::vector<const Foe*> order;
		for (const auto& kv : foes) order.push_back(&kv.second);
		// (in regions' order a region's leaders by their badges, one it doesn't know last)
		const bool byBadge = d.guideRegions;
		std::stable_sort(order.begin(), order.end(), [byBadge](const Foe* a, const Foe* b)
		{
			if (a->stage != b->stage) return a->stage < b->stage;
			if (byBadge && (a->stage == FIRST || a->stage == SECOND))
				return (a->badge ? a->badge : 99) < (b->badge ? b->badge : 99);
			return a->top < b->top;
		});

		// a leader whose party goes by the badge count: the one it will have when it's met
		const auto* gb = dynamic_cast<const GbRomData*>(&rom);
		const int count = badgeTeam(badges);
		int ahead = 0;	// (the gyms not yet beaten before it on the road)
		int gyms = 0;
		for (const Foe* f : order)
		{
			const nds::Trainer& t = *f->t;
			const bool gym = f->stage == FIRST || f->stage == SECOND;
			const std::vector<nds::TrainerMon>* team = &t.team;
			int top = f->top;
			if (gym)
			{
				const bool beaten = f->badge && (badges >> (f->badge - 1) & 1);
				const std::vector<std::vector<nds::TrainerMon>>* parties = nullptr;
				if (gb)
					if (auto p = gb->teams.find(t.id); p != gb->teams.end() && !p->second.empty()) parties = &p->second;
				if (parties)
				{
					const size_t k = (size_t)std::min(15, beaten ? count : count + ahead);
					team = &(*parties)[std::min(k, parties->size() - 1)];
					top = 0;
					for (const nds::TrainerMon& tm : *team) top = std::max(top, (int)tm.level);
				}
				if (!beaten) ahead++;
			}
			nds::Milestone m;
			m.trainerIds = f->ids;
			m.foeName = t.name;
			m.foeClass = t.cls;
			m.foeAceLevel = m.levelCap = top;
			m.q = nds::Quality::LIVE_VERIFIED;
			m.source = "the ROM's trainer table";
			// its type: the one most of the team has (as upstream's guideFromRom)
			std::map<int, int> types;
			for (const nds::TrainerMon& tm : *team)
				if (tm.species < rom.species.size())
				{
					const nds::Species& sp = rom.species[tm.species];
					types[sp.types[0]] += 2;
					if (sp.types[1] != sp.types[0]) types[sp.types[1]]++;
				}
			int most = 0;
			for (const auto& x : types)
				if (x.second > most && x.first < nds::NUM_TYPES) { most = x.second; m.theme = (nds::Type)x.first; }
			if (gym)
			{
				gyms++;
				m.kind = "gym";
				m.leader = t.name;
				// (in regions' order by its badge, which the order doesn't change, else by its place)
				m.id = "gym-" + std::to_string(byBadge && f->badge ? f->badge : gyms) + "-" + slug(t.name);
				m.name = "Gym " + std::to_string(gyms) + ": " + t.name;
				auto b = badgeOf.find(f->cls);
				if (b != badgeOf.end()) { m.badge = b->second; m.doneWhen = "badge:" + std::to_string(b->second); }
				else m.doneWhen = "later";
			}
			else
			{
				const bool elite = f->stage == ELITE;
				m.kind = elite ? "elite4" : "champion";
				m.id = (elite ? "e4-" : "champion-") + slug(t.name);
				m.name = (elite ? "Elite Four: " : "Champion: ") + t.name;
				m.doneWhen = "later";
			}
			g.milestones.push_back(std::move(m));
		}
		return g;
	}

	void romLevels(nds::GuideData& g, const nds::RomData& rom)
	{
		for (nds::Milestone& m : g.milestones)
		{
			if (m.trainerIds.empty() || m.levelCap > 0) continue;
			int top = 0;
			for (uint16_t id : m.trainerIds)
				if (auto t = rom.trainers.find(id); t != rom.trainers.end())
					for (const nds::TrainerMon& p : t->second.team) top = std::max<int>(top, p.level);
			m.levelCap = m.foeAceLevel = top;
		}
	}
}
