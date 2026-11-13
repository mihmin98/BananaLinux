// banana-ui (kiwami): the DS companion's pages for Generations I and II (the GB Pokémon companion): members of upstream's
// Pages::Impl, included inside the struct by a kiwami: hook in ui/src/nds/pages.cpp and defined in pages-impl.inc at
// that file's end. Each is called by a hook beside the block it replaces, and returns false (draws nothing) for
// upstream's games, so their pages are upstream's.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy

		// ---- kiwami: Generations I and II ---------------------------------------------------------------------------
		bool kiwamiOld() const;		// the game is Red, Blue, Yellow, Gold, Silver or Crystal
		// the Battle page's wild box: the DVs (and in Gen II Hidden Power), not a nature and IVs
		bool kiwamiWild(const Mon& f, float y, float bottom);
		// the Party page's rows beside the picture: Gen II's Item alone, Gen I nothing (no natures or abilities)
		bool kiwamiSummary(const Mon& m, float tx, float tw, float ly);
		bool kiwamiNoFriendship(const Mon& m) const;	// Gen I keeps none, but Yellow's Pikachu
		// the Party page's stats: DVs and stat experience, Gen I's Special one row
		bool kiwamiStats(const Mon& m, float x, float& y, float w);
		bool kiwamiNoAbilities() const;	// the Dex page: no Ability lines
		bool kiwamiBaseStats(const Species& sp, float x, float& y, float w);	// the Dex page: Gen I's five, Special once
		void kiwamiTrace(const char* what) const;	// KIWAMI_GBPK_TRACE=1: a line a block drawn (the tests)
