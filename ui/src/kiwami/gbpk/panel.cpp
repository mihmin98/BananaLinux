// banana-ui (kiwami): the Game Boy Pokémon companion on the game panel (panel.h), as upstream's gba/panel.cpp.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "panel.h"
#include "host.h"

namespace banana::kiwami::gbpk
{
	void Panel::open(Host* h, const gba::Panel::Look& look)
	{
		close();
		if (!h) return;
		host = h;
		pages = std::make_unique<nds::Pages>(look.be, look.screen, look.w, look.h, look.assets, look.light);
		pages->setDecoderThread(true);	// (the panel never waits on a picture's decode)
		pages->setBackLabel("Menu");	// (in the top-right corner, where the menu's icon is: the same spot switches)
		pages->show(nds::Page::PARTY);
		onPages = true;
		inBattle = false;
	}

	void Panel::close()
	{
		pages.reset();
		host = nullptr;
	}

	bool Panel::update(double now)
	{
		if (!active()) return false;
		bool changed = false;
		if (host->tick(now))
		{
			nds::PageData d;
			if (host->takeData(d)) pages->setData(d);
			changed = true;
		}
		host->markSeen(pages->takeSeen());
		if (pages->update(now)) changed = true;
		// a battle starting shows the battle, its end the party (unless another page was picked meanwhile)
		const bool b = host->inBattle();
		if (b != inBattle)
		{
			if (b) pages->show(nds::Page::BATTLE);
			else if (pages->page() == nds::Page::BATTLE) pages->show(nds::Page::PARTY);
			inBattle = b;
			changed = true;
		}
		return changed;
	}

	void Panel::draw()
	{
		if (pagesShown()) pages->draw();
	}

	void Panel::touch(bool down, int x, int y, double now)
	{
		if (!pagesShown()) return;
		const nds::Pages::Action a = down ? pages->touchDown(x, y, now) : pages->touchUp(x, y, now);
		if (a == nds::Pages::Action::CLOSE) onPages = false;
	}

	void Panel::touchMove(int x, int y, double now)
	{
		if (pagesShown()) pages->touchMove(x, y, now);
	}

	void Panel::showPages(double now)
	{
		if (!active()) return;
		if (!onPages) pages->opened(now);
		onPages = true;
	}

	int Panel::badge() const { return active() ? pages->unseen() : 0; }

	std::string Panel::view() const
	{
		if (!active()) return "";
		return pagesShown() ? std::string("pages:") + nds::pageName(pages->page()) : "game";
	}

	int Panel::ready() const { return !active() ? 0 : host->ready() ? 1 : host->unsupported() ? -2 : host->failed() ? -1 : 0; }
	std::string Panel::title() const { return active() ? host->title() : std::string(); }

	// ---- Settings' gbaPanel ----------------------------------------------------------------------------------------

	namespace { Host* handed = nullptr; }

	void PanelSwitch::pending(Host* host) { handed = host; }

	void PanelSwitch::open(gba::Host* host, pkmn::Companion* comp, const gba::Panel::Look& look)
	{
		gba.open(host, comp, look);
		gb.close();
		// (a Game Boy game's, once: the next game's panel is the next game's)
		if (!gba.active() && handed) gb.open(handed, look);
		handed = nullptr;
	}
}
