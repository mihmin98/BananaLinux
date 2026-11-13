#pragma once
// banana-ui (kiwami): Gold's and Silver's guide, built from the player's ROM. PokeDaisy has no guide for the two, so
// the road is the ROM's own trainers, as upstream's removed nds::guideFromRom made one for a GBA ROM hack (commit
// a103f12e): the gym leaders, the Elite Four and the Champion, each a milestone with its team's top level.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include "../../nds/model.h"

namespace banana::kiwami::gbpk
{
	// The milestones of the trainers whose class the ROM names Leader, Elite Four or Champion (a class a milestone,
	// with every trainer of it), in the story's stages and by level in each: Johto's leaders, the Elite Four, the
	// Champion, then Kanto's leaders. A leader is done by the badge its gym gives (the data file's leader<Name> class
	// and badge<Name> bit, from pret, paired here), not by its place in that order; a leader class it doesn't know
	// goes with Johto's and is done by a later milestone, as the Elite Four and the Champion are. No places, secrets
	// or notes. Empty when the ROM has none of these trainers.
	nds::GuideData romGuide(const nds::RomData& rom, const GameData& d);
}
