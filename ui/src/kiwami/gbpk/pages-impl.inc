// banana-ui (kiwami): the DS companion's pages for Generations I and II (pages-decl.inc), included at the end of
// ui/src/nds/pages.cpp by a kiwami: hook, after its namespace: the members are drawn with Pages::Impl's own helpers and
// look, in the place of upstream's blocks. Only header-only fork code is used (gbpk.h, rules.h): upstream's test
// binaries build pages.cpp without the fork's sources.
// What the games keep: DVs 0-15 (Mon::ivs: HP, Attack, Defense, Speed, Special twice; the HP DV made from the other
// four's lowest bits), stat experience 0-65535 (Mon::statExp: HP, Attack, Defense, Speed, Special), in Gen I one
// Special stat and no held items or friendship (but Yellow's Pikachu's), and no natures or abilities in either.
// Test switches: KIWAMI_GBPK_TRACE=1 logs a line for each block drawn, KIWAMI_GBPK_PAGES=0 draws upstream's blocks.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "gbpk.h"
#include "rules.h"
#include <cstdlib>
#include <cstring>

namespace banana::nds
{
	namespace
	{
		bool kiwamiEnv(const char* name, const char* value)
		{
			const char* v = getenv(name);
			return v && strcmp(v, value) == 0;
		}
		const char* const KIWAMI_GEN1_STATS[5] = { "HP", "Attack", "Defense", "Speed", "Special" };
	}

	bool Pages::Impl::kiwamiOld() const
	{
		static const bool off = kiwamiEnv("KIWAMI_GBPK_PAGES", "0");
		return !off && dt.rom && kiwami::gbpk::oldGen(dt.rom->game.gen);
	}

	void Pages::Impl::kiwamiTrace(const char* what) const
	{
		static const bool on = kiwamiEnv("KIWAMI_GBPK_TRACE", "1");
		if (on) fprintf(stderr, "banana-ui: gb companion: pages: %s\n", what);
	}

	bool Pages::Impl::kiwamiWild(const Mon& f, float y, float bottom)
	{
		if (!kiwamiOld()) return false;
		const bool gen2 = dt.rom->game.gen == kiwami::gbpk::GEN_II;
		box("bubble", 360, y, 268, bottom - y);
		text(tr("DVs"), true, 14, 374, y + 8, p.ink);
		// Hidden Power at the right, as upstream's, by Gen II's own formula from the DVs (Gen I has no Hidden Power)
		if (gen2)
		{
			const Type ht = (Type)kiwami::gbpk::hiddenPowerType(f.ivs);
			const std::string pw = std::to_string(kiwami::gbpk::hiddenPowerPower(f.ivs));
			const float pww = textR(pw, true, 13, 614, y + 8, p.ink, true);
			const float cw = typeChip(ht, 0, 0, true);
			typeChip(ht, 614 - pww - 6 - cw, y + 7);
			textR(tr("Hidden Power"), false, 12, 614 - pww - 12 - cw, y + 10, p.muted);
		}
		// the five DVs as columns (15 the top): HP, Attack, Defense, Speed, Special
		static const char* const SHORT[5] = { "HP", "Atk", "Def", "Spe", "Spc" };
		const float iy = y + 32, cw = 48, hgt = std::max(16.0f, bottom - y - 32 - 34);
		for (int i = 0; i < 5; i++)
		{
			const float cx = 374 + i * cw, bw = cw - 12;
			const int dv = f.ivs[i] & 15;
			const float f15 = dv / 15.0f;
			fill(cx, iy, bw, hgt, barTrack());
			fill(cx, iy + hgt * (1 - f15), bw, hgt * f15, dv >= 15 ? p.accent : dv >= 10 ? Color::hex(GOOD) : dv >= 5 ? Color::hex(HP_YELLOW) : Color::hex(BAD));
			const std::string v = std::to_string(dv);
			const float vw = measure(v, true, 12, true), lw = measure(SHORT[i], false, 11, true);
			text(v, true, 12, cx + bw / 2 - vw / 2, iy + hgt + 2, p.ink, true);
			text(SHORT[i], false, 11, cx + bw / 2 - lw / 2, iy + hgt + 16, p.muted, true);
		}
		kiwamiTrace(gen2 ? "wild dvs hidden-power" : "wild dvs");
		return true;
	}

	bool Pages::Impl::kiwamiSummary(const Mon& m, float tx, float tw, float ly)
	{
		if (!kiwamiOld()) return false;
		if (m.egg || dt.rom->game.gen != kiwami::gbpk::GEN_II) { if (!m.egg) kiwamiTrace("summary none"); return true; }
		// upstream's Item row, alone (its Nature and Ability rows above it left out)
		const std::string it = itemName(m.item);
		const float lw = text(tr("Item"), false, 12, tx, ly + 1, p.muted);
		text(it.empty() ? tr("none") : it, true, 13, tx + std::max(56.0f, lw + 8), ly, p.ink, !it.empty(), tw - 60);
		kiwamiTrace("summary item");
		return true;
	}

	bool Pages::Impl::kiwamiNoFriendship(const Mon& m) const
	{
		if (!kiwamiOld() || dt.rom->game.gen != kiwami::gbpk::GEN_I) return false;
		if (dt.rom->game.id == kiwami::gbpk::YELLOW && m.species == 25) return false;	// (Pikachu's, kept by Yellow)
		kiwamiTrace("no friendship");
		return true;
	}

	bool Pages::Impl::kiwamiStats(const Mon& m, float x, float& y, float w)
	{
		if (!kiwamiOld()) return false;
		const bool gen1 = dt.rom->game.gen == kiwami::gbpk::GEN_I;
		const int n = gen1 ? 5 : 6;
		// upstream's columns: the stat, then the DV where the IV is, stat experience where the effort points are
		y = heading(tr("Stats"), x, y);
		text(tr("DV"), false, 11, x + 146, y - 20, p.muted);
		text(tr("Stat exp."), false, 11, x + 214, y - 20, p.muted);
		for (int i = 0; i < n; i++)
		{
			// the rows: HP, Attack, Defense, Speed, then Sp. Atk and Sp. Def (Gen II, both from Special's DV and stat
			// experience) or Special (Gen I: stats[3], which the reader also copies to stats[4])
			const int val = i == 0 ? m.maxHp : m.stats[i - 1];
			const int dv = m.ivs[i] & 15, se = m.statExp[std::min(i, 4)];
			const float ry = y + i * 20;
			text(gen1 ? KIWAMI_GEN1_STATS[i] : STAT_NAMES[i], false, 12, x, ry + 1, p.muted);
			text(std::to_string(val), true, 13, x + 62, ry, p.ink, true);
			// the DV: a number, the accent at 15
			const std::string d = std::to_string(dv);
			text(d, true, 12, x + 140 - measure(d, true, 12, true), ry + 1, dv == 15 ? p.accent : p.ink, true);
			bar(x + 146, ry + 5, 50, 6, dv / 15.0f, dv >= 12 ? Color::hex(GOOD) : dv >= 5 ? Color::hex(HP_YELLOW) : Color::hex(BAD));
			// stat experience out of 65535 (the games' cap)
			bar(x + 214, ry + 5, w - 214 - 46, 6, se / 65535.0f, Color::hex(EXP_BLUE));
			textR(std::to_string(se), true, 12, x + w, ry + 1, p.ink, true);
		}
		y += n * 20 + 10;	// (no total: stat experience has no cap over the five)
		kiwamiTrace(gen1 ? "stats dv statexp 5 rows" : "stats dv statexp 6 rows");
		return true;
	}

	bool Pages::Impl::kiwamiNoAbilities() const
	{
		if (!kiwamiOld()) return false;
		kiwamiTrace("no abilities");
		return true;
	}

	bool Pages::Impl::kiwamiBaseStats(const Species& sp, float x, float& y, float w)
	{
		if (!kiwamiOld() || dt.rom->game.gen != kiwami::gbpk::GEN_I) return false;
		// upstream's block with five rows: Special once (the ROM reader keeps it in base[4] and base[5])
		y = heading(tr("Base stats"), x, y);
		int total = 0;
		for (int i = 0; i < 5; i++)
		{
			total += sp.base[i];
			const float ry = y + i * 18;
			text(KIWAMI_GEN1_STATS[i], false, 12, x, ry, p.muted);
			textR(std::to_string(sp.base[i]), true, 12, x + 96, ry, p.ink, true);
			bar(x + 104, ry + 4, w - 104, 7, sp.base[i] / 180.0f, sp.base[i] >= 100 ? Color::hex(GOOD) : sp.base[i] >= 60 ? Color::hex(HP_YELLOW) : Color::hex(BAD));
		}
		y += 5 * 18;
		text(tr("Total"), false, 12, x, y, p.muted);
		textR(std::to_string(total), true, 12, x + 96, y, p.ink, true);
		y += 26;
		kiwamiTrace("base stats special");
		return true;
	}
}
