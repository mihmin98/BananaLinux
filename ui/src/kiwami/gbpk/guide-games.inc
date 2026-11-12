// banana-ui (kiwami): the Game Boy and Game Boy Color Pokémon games in upstream's guide loader (ui/src/nds/guide.cpp's
// GAMES, included there by a kiwami: hook): their ids (gbpk.h's, as literals: guide.cpp includes no fork header), the
// key their guide files and data files are named by, a code each file must give (Gold's, Silver's and Crystal's
// headers'; Red, Blue and Yellow have none, so theirs are the fork's own), and the Pokédex's size. The files are in
// /usr/share/banana/companions/gb-pokemon/, loaded by the fork's host (host.cpp), not by nds::loadGuide.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
			{ (GameId)100, "red", "PKRD", 151 }, { (GameId)101, "blue", "PKBL", 151 }, { (GameId)102, "yellow", "PKYL", 151 },
			{ (GameId)103, "gold", "AAUE", 251 }, { (GameId)104, "silver", "AAXE", 251 }, { (GameId)105, "crystal", "BYTE", 251 },
