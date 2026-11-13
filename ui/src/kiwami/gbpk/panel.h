#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion on the game panel, as upstream's gba::Panel shows the GBA one: the
// DS companion's pages (nds/pages.h) with gbpk::Host's data, switched with the panel's menu the same way (the corner,
// the back button, F the menu). They show first: "Reading your game" till the ROM's data is read, "No game data" when
// it can't be, "Not supported" alone for a release the companion doesn't know. A battle starting shows the Battle
// page, its end the Party page.
// PanelSwitch is the type of Settings' gbaPanel (a kiwami: hook in settings.h): upstream's gba::Panel, with the Game
// Boy one beside it, every member upstream calls going to whichever is open (upstream's first). The Game Boy panel
// opens when Settings attaches the game's panel (gbaAttach) with a Game Boy host handed over just before (pending,
// by game.h's Game::attach), once.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "../../gba/panel.h"
#include "../../nds/pages.h"
#include <memory>
#include <string>

namespace banana::kiwami::gbpk
{
	class Host;

	class Panel
	{
	public:
		void open(Host* host, const gba::Panel::Look& look);	// the pages show at once
		void close();		// (before the host goes)
		bool active() const { return host && pages; }
		bool pagesShown() const { return active() && onPages; }
		bool update(double nowMs);
		void draw();
		void touch(bool down, int x, int y, double nowMs);
		void touchMove(int x, int y, double nowMs);
		void showPages(double nowMs);
		void showMenu() { onPages = false; }
		void toggle(double nowMs) { if (pagesShown()) showMenu(); else showPages(nowMs); }
		int badge() const;
		std::string view() const;	// (tests) "pages:<page>", "game" (the menu) or "" without a game
		int ready() const;			// (tests) 1 read, -1 couldn't be, -2 not supported, 0 not yet (or no game)
		std::string title() const;

	private:
		Host* host = nullptr;
		std::unique_ptr<nds::Pages> pages;
		bool onPages = true, inBattle = false;
	};

	// Settings' gbaPanel: upstream's GBA panel or the Game Boy one
	class PanelSwitch
	{
	public:
		// a Game Boy game's host, for the next open (null: none); Game::attach hands it over before Settings attaches
		static void pending(Host* host);

		void open(gba::Host* host, pkmn::Companion* comp, const gba::Panel::Look& look);
		void close() { gba.close(); gb.close(); }
		bool active() const { return gba.active() || gb.active(); }
		bool pagesShown() const { return gba.active() ? gba.pagesShown() : gb.pagesShown(); }
		bool update(double nowMs) { return gba.active() ? gba.update(nowMs) : gb.update(nowMs); }
		void draw() { if (gba.active()) gba.draw(); else gb.draw(); }
		void touch(bool down, int x, int y, double nowMs) { if (gba.active()) gba.touch(down, x, y, nowMs); else gb.touch(down, x, y, nowMs); }
		void touchMove(int x, int y, double nowMs) { if (gba.active()) gba.touchMove(x, y, nowMs); else gb.touchMove(x, y, nowMs); }
		void showPages(double nowMs) { if (gba.active()) gba.showPages(nowMs); else gb.showPages(nowMs); }
		void showMenu() { if (gba.active()) gba.showMenu(); else gb.showMenu(); }
		void toggle(double nowMs) { if (gba.active()) gba.toggle(nowMs); else gb.toggle(nowMs); }
		int badge() const { return gba.active() ? gba.badge() : gb.badge(); }
		std::string view() const { return gba.active() ? gba.view() : gb.view(); }
		int ready() const { return gba.active() ? gba.ready() : gb.ready(); }
		std::string title() const { return gba.active() ? gba.title() : gb.title(); }
		// the Game Boy panel is the one open: Back (the button beside F, which a Game Boy game has no use for) switches
		// it between the menu and the pages, as Back's Companion job does in a GBA game (a hook in Settings::gameButton)
		bool gameBoy() const { return !gba.active() && gb.active(); }

	private:
		gba::Panel gba;
		Panel gb;
	};
}
