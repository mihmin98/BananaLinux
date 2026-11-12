// banana-ui (kiwami): Gold's and Silver's guide from the ROM's trainers (gsguide.h).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "gsguide.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// which badge each leader's gym gives: the gym's script's setflag ENGINE_<badge> (pret's pokegold maps/*Gym.asm,
		// Clair's maps/DragonsDenB1F.asm). The class ids and the badges' bits are the data file's, from pret's
		// constants (kiwami/tools/gbpk-symbols.txt); a Kanto badge is a bit of wKantoBadges, Live::badges' high byte
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

	nds::GuideData romGuide(const nds::RomData& rom, const GameData& d)
	{
		nds::GuideData g;
		g.game = d.key;
		g.title = "Pokémon " + d.game.title;
		g.sources = { "The player's ROM: its gym leaders, Elite Four and Champion from the trainer table, each leader's badge "
			"by pret/pokegold's constants (kiwami/GB-POKEMON.md)" };

		// the leader classes' badges: 1-8 Johto's, 9-16 Kanto's (done_when badge:N)
		std::map<int, int> badgeOf;
		for (const Leader& l : LEADERS)
		{
			const int cls = d.size(l.cls, -1), bit = d.size(l.badge, -1);
			if (cls > 0 && bit >= 0 && bit < 8) badgeOf[cls] = (l.kanto ? 8 : 0) + bit + 1;
		}

		// the stages, in the story's order
		enum { JOHTO, ELITE, CHAMPION, KANTO };
		struct Foe { int stage = JOHTO, top = 999, cls = 0; std::vector<uint16_t> ids; const nds::Trainer* t = nullptr; };
		std::map<int, Foe> foes;	// by class
		for (const auto& kv : rom.trainers)
		{
			const nds::Trainer& t = kv.second;
			const std::string c = lower(t.cls);
			const int cls = t.id >> 8;
			int stage;
			if (c == "leader") { auto b = badgeOf.find(cls); stage = b != badgeOf.end() && b->second > 8 ? KANTO : JOHTO; }
			else if (c == "elite four") stage = ELITE;
			else if (c == "champion") stage = CHAMPION;
			else continue;
			if (t.name.empty() || t.team.empty()) continue;
			int top = 0;
			for (const nds::TrainerMon& m : t.team) top = std::max(top, (int)m.level);
			Foe& f = foes[cls];
			f.stage = stage;
			f.cls = cls;
			f.ids.push_back(t.id);
			if (top < f.top) { f.top = top; f.t = &t; }	// (the first fight: its lowest-levelled team)
		}
		std::vector<const Foe*> order;
		for (const auto& kv : foes) order.push_back(&kv.second);
		std::stable_sort(order.begin(), order.end(), [](const Foe* a, const Foe* b)
			{ return a->stage != b->stage ? a->stage < b->stage : a->top < b->top; });

		int gyms = 0;
		for (const Foe* f : order)
		{
			const nds::Trainer& t = *f->t;
			nds::Milestone m;
			m.trainerIds = f->ids;
			m.foeName = t.name;
			m.foeClass = t.cls;
			m.foeAceLevel = m.levelCap = f->top;
			m.q = nds::Quality::LIVE_VERIFIED;
			m.source = "the ROM's trainer table";
			// its type: the one most of the team has (as upstream's guideFromRom)
			std::map<int, int> types;
			for (const nds::TrainerMon& tm : t.team)
				if (tm.species < rom.species.size())
				{
					const nds::Species& sp = rom.species[tm.species];
					types[sp.types[0]] += 2;
					if (sp.types[1] != sp.types[0]) types[sp.types[1]]++;
				}
			int most = 0;
			for (const auto& x : types)
				if (x.second > most && x.first < nds::NUM_TYPES) { most = x.second; m.theme = (nds::Type)x.first; }
			if (f->stage == JOHTO || f->stage == KANTO)
			{
				gyms++;
				m.kind = "gym";
				m.leader = t.name;
				m.id = "gym-" + std::to_string(gyms) + "-" + slug(t.name);
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
}
