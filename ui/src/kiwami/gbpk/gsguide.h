#pragma once
// banana-ui (kiwami): Gold's, Silver's and Crystal Clear's guide, built from the player's ROM, and a guide file's levels
// from it. PokeDaisy has no guide for the three, so the road is the ROM's own trainers, as upstream's removed
// nds::guideFromRom made one for a GBA ROM hack (commit a103f12e): the gym leaders, the Elite Four and the Champion, each
// a milestone with its team's top level.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include "../../nds/model.h"

namespace banana::kiwami::gbpk
{
	// The milestones of the trainers whose class the ROM names Leader, and of the data file's Elite Four and Champion
	// classes (eliteWill .. eliteKoga, champion), a class a milestone with every trainer of it, in the story's stages:
	// Johto's leaders, the Elite Four, the Champion, then Kanto's leaders, by level in each. A leader is done by the
	// badge its gym gives (the data file's leader<Name> class and badge<Name> bit, from pret, paired here), not by its
	// place in that order; a leader class it doesn't know goes with Johto's and is done by a later milestone, as the
	// Elite Four and the Champion are. No places, secrets or notes. Empty when the ROM has none of these trainers.
	// In regions' order (d.guideRegions: Crystal Clear, which starts in either region) the save's badges (Live::badges,
	// Kanto's the high byte) give the first region, Kanto's when it has more of its badges than of Johto's, and the
	// leaders go in their badges' order in each. A leader whose party goes by the badge count (GbRomData::teams) is
	// shown with the one it will have: the k-th gym not yet beaten on the road with its party for the save's count + k
	// (rom.h badgeTeam), one beaten with the count's.
	nds::GuideData romGuide(const nds::RomData& rom, const GameData& d, uint16_t badges = 0);

	// A guide file's bosses at the ROM's levels (a data file's "guide_levels": "rom": PureRGB's guides, Red's and Blue's
	// with its own teams): each milestone that names its trainers (trainer_ids) and no level cap gets the highest level
	// of their teams in the ROM as its level cap and its foe's ace. Trainers the ROM hasn't are passed over; a milestone
	// none of whose trainers it has keeps no level.
	void romLevels(nds::GuideData& g, const nds::RomData& rom);
}
