#pragma once
// banana-ui (kiwami): Generations I and II's battle rules, for upstream's logic (ui/src/nds/logic.cpp) and type chart
// (ui/src/nds/model.cpp), which call them through marked hooks for the Game Boy games (gbpk.h's GEN_I and GEN_II):
// the type chart, the damage, the Speed and the catch chance, as the games' own code works them out (pret's pokered
// and pokecrystal: engine/battle/core.asm, effect_commands.asm, misc.asm, engine/items/item_effects.asm, named at
// each rule). Header-only: upstream's own test binaries link ui/src/nds/ without the fork's sources.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "gbpk.h"
#include "../../nds/logic.h"
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace banana::kiwami::gbpk
{
	// ---- the game in use ----------------------------------------------------------------------------------------
	// Set by the companion's host when a Game Boy game's data is read, and put back (useRules with an upstream Gen)
	// when it closes, as upstream's useModernTypeChart is: one game at a time. oldChart: Gen I's type chart in
	// upstream's effectiveness(); oldRules: a Game Boy game's rules, for upstream's effectiveSpeed(), whose Gen isn't
	// named. badges: the player's (Johto's, then Kanto's), for Gen II's badge type boosts.
	inline std::atomic<bool> oldChart{ false }, oldRulesOn{ false };
	inline std::atomic<uint16_t> badges{ 0 };
	inline void useRules(nds::Gen g) { oldChart.store(g == GEN_I); oldRulesOn.store(oldGen(g)); badges.store(0); }
	inline bool oldRules() { return oldRulesOn.load(); }
	inline void setBadges(uint16_t b) { badges.store(b); }

	// ---- the type chart ---------------------------------------------------------------------------------------------
	// a cell of Gen I's chart in quarters where it differs from upstream's (which is Gen II's: pret's pokecrystal
	// data/types/type_matchups.asm), -1 for upstream's own. Gen I's (pokered data/types/type_matchups.asm) has no Steel
	// or Dark, Ghost does nothing to Psychic, Bug and Poison are super effective on each other, Ice is neutral on Fire.
	// Only while oldChart is on.
	inline int chart(nds::Type a, nds::Type d)
	{
		using namespace nds;
		if (!oldChart.load()) return -1;
		if (a == STEEL || a == DARK || d == STEEL || d == DARK) return 4;
		if (a == GHOST && d == PSYCHIC) return 0;
		if ((a == BUG && d == POISON) || (a == POISON && d == BUG)) return 8;
		if (a == ICE && d == FIRE) return 4;
		return -1;
	}

	// ---- items (their Gen IV numbers, as the reader gives them: items.inc) -------------------------------------------
	enum : uint16_t { MASTER_BALL = 1, ULTRA_BALL = 2, GREAT_BALL = 3, POKE_BALL = 4, SAFARI_BALL = 5,
		SILVERPOWDER = 222, METAL_COAT = 233, DRAGON_SCALE = 235, LIGHT_BALL = 236, THICK_CLUB = 258,
		FAST_BALL = 492, LEVEL_BALL = 493, LURE_BALL = 494, HEAVY_BALL = 495, LOVE_BALL = 496, FRIEND_BALL = 497,
		MOON_BALL = 498, CONTEST_BALL = 499,	// (the Bug-Catching Contest's Park Ball: Gen IV's Sport Ball)
		PINK_BOW = 1000 + 0x68, POLKADOT_BOW = 1000 + 0xAA };

	// the type a held item powers up by 10 % in Gen II (pokecrystal data/types/type_boost_items.asm; each 10 in
	// data/items/attributes.asm), NO_TYPE for none. Dragon Scale is Dragon's; Dragon Fang (250) does nothing there.
	inline nds::Type boostType(uint16_t item)
	{
		using namespace nds;
		static const Type T237[] = { GROUND, ROCK, GRASS, DARK, FIGHTING, ELECTRIC, WATER, FLYING, POISON, ICE, GHOST,
			PSYCHIC, FIRE };	// Soft Sand .. Charcoal (237-249)
		if (item >= 237 && item <= 249) return T237[item - 237];
		if (item == SILVERPOWDER) return BUG;
		if (item == METAL_COAT) return STEEL;
		if (item == DRAGON_SCALE) return DRAGON;
		if (item == PINK_BOW || item == POLKADOT_BOW) return NORMAL;
		return NO_TYPE;
	}

	// Gen II's badge type boosts (pokecrystal data/types/badge_type_boosts.asm): by badge bit (Johto's eight, then
	// Kanto's), the type whose moves the player's Pokémon use for 1/8 more
	inline nds::Type badgeType(int bit)
	{
		using namespace nds;
		static const Type T[16] = { FLYING, BUG, NORMAL, GHOST, STEEL, FIGHTING, ICE, DRAGON,
			ROCK, WATER, ELECTRIC, GRASS, POISON, PSYCHIC, FIRE, GROUND };
		return bit >= 0 && bit < 16 ? T[bit] : NO_TYPE;
	}

	// ---- damage -------------------------------------------------------------------------------------------------
	// The games' formula: pokered engine/battle/core.asm (GetDamageVarsForPlayerAttack, CalculateDamage,
	// AdjustDamageForMoveType, RandomizeDamage), pokecrystal engine/battle/effect_commands.asm (PlayerAttackDamage,
	// BattleCommand_DamageCalc, BattleCommand_Stab, BattleCommand_DamageVariation) and misc.asm (DoBadgeTypeBoosts).
	//   physical or special by the move's type (Fire and after: special; Gen I's Special is both special stats);
	//   the battle's own Attack and Defense: the battle copy's stats already hold the stat stages, the badges' stat
	//   boosts, burn's halved Attack and paralysis's quartered Speed (the games recompute them), so none is applied
	//   again; Gen II: Thick Club doubles Cubone's and Marowak's Attack, Light Ball Pikachu's Special Attack;
	//   either above 255: both / 4 (Gen I once, Gen II until both fit), at least 1;
	//   ((2 * level / 5 + 2) * power * Attack / Defense) / 50; Gen II: the type item's + 10 %; at most 997, then + 2;
	//   Gen II: the badge's + 1/8 (at least 1) for the player's moves (upstream's logic asks only for those);
	//   STAB: + 1/2; each type matchup: * 2 or / 2 (the games walk their chart: here the defender's first type, then
	//   its second, which rounds the same but for a 1 in rare dual-type cases);
	//   the random factor: * 217..255 / 255, when the damage is 2 or more.
	// Not counted, as upstream's DS logic doesn't: critical hits, Reflect and Light Screen, weather (upstream passes
	// none), Explosion's halved Defense.
	inline nds::DamageRange damage(nds::Gen g, const nds::MoveData& mv, const nds::Fighter& a, const nds::Fighter& d, nds::Weather)
	{
		using namespace nds;
		DamageRange r;
		if (mv.cat == MoveCategory::STATUS || mv.power == 0) { r.note = "no damage"; r.quarters = 4; return r; }
		const Type t = mv.type;
		const int e1 = d.types[0] == NO_TYPE ? 4 : effectiveness(t, d.types[0], NO_TYPE);
		const int e2 = d.types[1] == NO_TYPE || d.types[1] == d.types[0] ? 4 : effectiveness(t, d.types[1], NO_TYPE);
		r.quarters = e1 * e2 / 4;
		if (r.quarters == 0) { r.note = "no effect"; return r; }
		if (mv.power <= 1) { r.note = "power varies"; return r; }
		const bool phys = t < FIRE;
		int atk = phys ? a.stats[0] : a.stats[3], def = phys ? d.stats[1] : d.stats[4];
		if (g == GEN_II)
		{
			if (phys && a.item == THICK_CLUB && (a.species == 104 || a.species == 105)) atk *= 2;
			if (!phys && a.item == LIGHT_BALL && a.species == 25) atk *= 2;
		}
		do
		{
			if (atk <= 255 && def <= 255) break;
			atk = std::max(1, atk / 4);
			def = std::max(1, def / 4);
		} while (g == GEN_II);
		atk = std::max(1, std::min(255, atk));
		def = std::max(1, std::min(255, def));
		int dmg = ((2 * a.level / 5 + 2) * mv.power * atk / def) / 50;
		if (g == GEN_II && boostType(a.item) == t) dmg = dmg * 110 / 100;
		dmg = std::min(dmg, 997) + 2;
		if (g == GEN_II)
		{
			const uint16_t held = badges.load();
			for (int bit = 0; bit < 16; bit++)
				if ((held >> bit) & 1 && badgeType(bit) == t) { dmg += std::max(1, dmg / 8); break; }
		}
		r.stab = t == a.types[0] || t == a.types[1];
		if (r.stab) dmg += dmg / 2;
		if (e1 != 4) dmg = dmg * e1 / 4;
		if (e2 != 4) dmg = dmg * e2 / 4;
		r.hi = dmg;
		r.lo = dmg >= 2 ? dmg * 217 / 255 : dmg;
		return r;
	}

	// the Speed the games compare: the battle copy's, which holds the stages and paralysis already
	inline int speed(const nds::Fighter& f) { return f.stats[2]; }

	// ---- catching -------------------------------------------------------------------------------------------------
	// Gen I (pokered engine/items/item_effects.asm ItemUseBall): a first random number R1 in 0..N (Poké Ball 255,
	// Great 200, Ultra and Safari 150) less the status' S (sleep, freeze 25; burn, paralysis, poison 12) catches outright
	// below 0, fails above the catch rate; else W = (MaxHP * 255 / F) / max(HP / 4, 1) (F: Great 8, the others 12)
	// catches above 255, else a second random byte catches at most W. Worked out as the probability.
	inline float catchGen1(const nds::Species& s, const nds::Mon& wild, uint16_t ball, float* mult)
	{
		const int n = ball == GREAT_BALL ? 200 : ball == ULTRA_BALL || ball == SAFARI_BALL ? 150 : 255;
		const int f = ball == GREAT_BALL ? 8 : 12;
		if (mult) *mult = ball == GREAT_BALL ? 1.5f : ball == ULTRA_BALL || ball == SAFARI_BALL ? 2 : 1;
		const uint32_t st = wild.status;
		const int sv = (st & 7) || (st & 0x20) ? 25 : (st & 0xFF) ? 12 : 0;
		const int m = wild.maxHp, h = std::min<int>(wild.hp, m);
		const int w = (m * 255 / f) / std::max(1, (h / 4) & 0xFF);
		const double p2 = w > 255 ? 1.0 : (w + 1) / 256.0;
		const int sure = std::min(sv, n + 1);								// R1 < S
		const int pass = std::max(0, std::min(n, sv + s.catchRate) - sv + 1);	// S <= R1 <= S + rate
		return (float)std::min(1.0, (sure + pass * p2) / (n + 1));
	}

	// Gen II (pokecrystal engine/items/item_effects.asm PokeBallEffect and the balls' multipliers, their bugs kept):
	// the ball changes the catch rate; then (3 * MaxHP - 2 * HP) * rate / (3 * MaxHP) in the game's bytes (both / 4
	// when 3 * MaxHP passes 255, then their low bytes); sleep and freeze add 10 (burn, paralysis and poison nothing);
	// at most 255; a random byte at most it catches: (rate + 1) / 256. The Level Ball skips the HP and the status.
	inline float catchGen2(const nds::RomData& rom, const nds::Species& s, const nds::Mon& wild, uint16_t ball,
		const nds::CatchContext& c, float* mult)
	{
		int b = s.catchRate;
		const int rate0 = std::max(1, b);
		auto cap = [](int v) { return std::min(255, v); };
		switch (ball)
		{
		case ULTRA_BALL: b = cap(b * 2); break;
		case GREAT_BALL: case SAFARI_BALL: case CONTEST_BALL: b = cap(b + b / 2); break;
		case HEAVY_BALL:
		{
			// by the Pokédex weight (0.1 kg, as the pages keep it; the game converts its 0.1 lb on the way): under 102.4 kg
			// - 20 (a rate under 20: 1), under 204.8 + 0, 307.2 + 20, 409.6 + 30, else + 40
			const int hi = s.weight >> 8;
			if (hi < 4) b = b >= 20 ? b - 20 : 1;
			else b = cap(b + (hi < 8 ? 0 : hi < 12 ? 20 : hi < 16 ? 30 : 40));
			break;
		}
		case LURE_BALL: if (c.fishing) b = cap(b * 3); break;
		case FAST_BALL:		// (its bug: only the first three species of the fleeing ones: Magnemite, Grimer, Tangela)
			if (wild.species == 81 || wild.species == 88 || wild.species == 114) b = cap(b * 4);
			break;
		case LOVE_BALL:		// (its bug: the same species of the same gender, neither genderless)
			if (c.ourSpecies == wild.species && c.ourFemale == wild.female && wild.species < rom.species.size() &&
				rom.species[wild.species].genderRatio != 255)
				b = cap(b * 8);
			break;
		case LEVEL_BALL:
		{
			const int me = c.ourLevel, them = wild.level;
			if (them < me) { b = cap(b * 2); if (them < me / 2) { b = cap(b * 2); if (them < me / 4) b = cap(b * 2); } }
			if (mult) *mult = (float)b / rate0;
			return (b + 1) / 256.0f;
		}
		default: break;		// Poké Ball, Friend Ball, Moon Ball (its bug: it never boosts)
		}
		if (mult) *mult = (float)b / rate0;
		int m3 = 3 * wild.maxHp, h2 = 2 * std::min<int>(wild.hp, wild.maxHp);
		int lowH2 = h2 & 0xFF;
		if (m3 > 255)
		{
			m3 >>= 2;
			h2 >>= 2;
			lowH2 = h2 & 0xFF;
			if (!lowH2) lowH2 = 1;
		}
		const int e = std::max(1, m3 & 0xFF);
		int x = b * ((e - lowH2) & 0xFF) / e;
		x &= 0xFF;
		if (!x) x = 1;
		if ((wild.status & 7) || (wild.status & 0x20)) x = cap(x + 10);
		return (x + 1) / 256.0f;
	}

	inline float catchChance(nds::Gen g, const nds::RomData& rom, const nds::Mon& wild, uint16_t ball,
		const nds::CatchContext& c, float* mult)
	{
		if (!wild.species || wild.species >= rom.species.size() || !wild.maxHp) return 0;
		if (ball == MASTER_BALL) { if (mult) *mult = 255; return 1; }
		const nds::Species& s = rom.species[wild.species];
		return g == GEN_I ? catchGen1(s, wild, ball, mult) : catchGen2(rom, s, wild, ball, c, mult);
	}

	// ---- Hidden Power and the stats (Gen II) ------------------------------------------------------------------------
	// pokecrystal engine/battle/hidden_power.asm, from the DVs as the reader keeps them in Mon::ivs (Attack 1, Defense
	// 2, Speed 3, Special 4): the type from Attack's and Defense's lowest two bits, Fighting (0) to Dark (15), which is
	// nds::Type's order + 1; the power from the four DVs' top bits and Special's lowest two, 31 to 70
	inline int hiddenPowerType(const uint8_t dvs[6]) { return 1 + ((dvs[1] & 3) << 2 | (dvs[2] & 3)); }
	inline int hiddenPowerPower(const uint8_t dvs[6])
	{
		const int top = (dvs[1] >> 3 & 1) << 3 | (dvs[2] >> 3 & 1) << 2 | (dvs[3] >> 3 & 1) << 1 | (dvs[4] >> 3 & 1);
		return (top * 5 + (dvs[4] & 3)) / 2 + 31;
	}
	// a stat from its base, DV, stat experience and level (pokecrystal engine/pokemon/move_mon.asm CalcMonStatC, Gen I's
	// the same): ((base + DV) * 2 + root / 4) * level / 100, + 5 (HP: + level + 10), where root is the first n whose
	// square isn't below the stat experience (at most 255); at most 999
	inline int statFrom(int base, int dv, int statExp, int level, bool hp)
	{
		int root = 1;
		while (root < 255 && root * root < statExp) root++;
		const int v = ((base + dv) * 2 + root / 4) * level / 100 + (hp ? level + 10 : 5);
		return std::min(v, 999);
	}
}
