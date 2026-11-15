// banana-ui (kiwami): the Game Boy Pokémon companion's pages' data (host.h), as upstream's gba/host.cpp.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "host.h"
#include "gsguide.h"
#include "rom.h"
#include "rules.h"
#include "../../nds/guide.h"
#include <chrono>
#include <cstdio>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// the game's guide: its file in the data folder (<key>.json, the places' maps found by name in the ROM), or for
		// a game whose data file says "guide": "rom" or "rom-regions" (Gold, Silver and Crystal Clear, which have none)
		// the one its ROM's trainers make for the save's badges (gsguide.h), and its bosses at the ROM's levels when it
		// says "guide_levels": "rom" (PureRGB's); with the ROM's hidden items as
		// the Here page's secrets, each listed whether found or not, as upstream's companions list theirs. A game
		// without a file (or with one it refuses) has the hidden items alone.
		std::shared_ptr<const nds::GuideData> guideFor(const GameData& d, const nds::RomData& rom, uint16_t badges = 0)
		{
			auto g = std::make_shared<nds::GuideData>();
			const nds::GuideResolver res = nds::guideResolver(rom);
			std::string why;
			if (d.romGuide)
				*g = romGuide(rom, d, badges);
			else if (!nds::loadGuideFile(dataDir() + "/" + d.key + ".json", *g, d.game.id, &why, &res))
			{
				fprintf(stderr, "banana-ui: gb companion: no guide: %s\n", why.c_str());
				*g = nds::GuideData();
			}
			else
			{
				if (d.romLevels) romLevels(*g, rom);
				if (!why.empty()) fprintf(stderr, "banana-ui: gb companion: guide: %s\n", why.c_str());
				for (const std::string& p : g->unresolved) fprintf(stderr, "banana-ui: gb companion: guide: no map is %s\n", p.c_str());
			}
			size_t hidden = 0;
			if (const auto* gb = dynamic_cast<const GbRomData*>(&rom))
				for (const HiddenItem& h : gb->hidden)
				{
					nds::Secret s;
					s.id = "hidden-" + std::to_string(h.flag);
					s.kind = "hidden-item";
					const std::string item = h.item < rom.items.size() ? rom.items[h.item].name : "";
					s.text = item.empty() ? "A hidden item." : "A hidden " + item + ".";
					s.zone = h.zone;
					s.zones = { h.zone };
					auto z = rom.zones.find(h.zone);
					s.place = z == rom.zones.end() ? "" : z->second.name;
					s.x = h.x;
					s.y = h.y;
					s.source = "the ROM's map events";
					g->secrets.push_back(std::move(s));
					hidden++;
				}
			fprintf(stderr, "banana-ui: gb companion: guide: %zu milestones, %zu hidden items\n", g->milestones.size(), hidden);
			return g;
		}
	}

	Host::Host(std::vector<GameData> ds, std::string rom, Reader& r, std::string sd, bool ch, std::shared_ptr<const nds::RomData> si)
		: games(std::move(ds)), data(games.empty() ? GameData() : games[0]), romPath(std::move(rom)), stateDir(std::move(sd)),
		reader(r), challenge(ch), standIn(std::move(si)) {}

	Host::~Host()
	{
		session.reset();	// (stopped first: it saves the journey on this thread)
		if (load.valid()) load.wait();
		reader.close();
		if (rules)	// (the Session has stopped: nothing asks the chart now)
		{
			useGameRules(nullptr);
			setSwitches(0, false);
			rules.reset();
		}
		// (upstream's hosts don't put the chart back: an Emerald after Red would be judged by Gen I's)
		if (base.rom)
		{
			useRules(nds::Gen::IV);
			fprintf(stderr, "banana-ui: gb companion: rules off\n");
		}
	}

	bool Host::tick(double now)
	{
		(void)now;
		// (till the ROM's data is read: the pages say so)
		if (!started)
		{
			started = true;
			out = nds::PageData();
			out.reading = true;
			fresh = true;
			if (standIn) begin({ standIn, data, false, guideFor(data, *standIn) });
			else
			{
				// the ROM: the whole file (or the game in its .zip or .7z), its SHA-1 among the games' releases (which
				// picks the game), then its tables
				loading = true;
				load = std::async(std::launch::async, [ds = games, path = romPath]() mutable
				{
					Loaded l;
					const auto t0 = std::chrono::steady_clock::now();
					auto rom = std::make_shared<std::vector<uint8_t>>(readRomFile(path, 8u << 20));
					const int k = confirmAny(*rom, ds);
					if (k < 0)
					{
						std::string t;
						for (const GameData& g : ds) t += (t.empty() ? "" : " or ") + g.game.title;
						fprintf(stderr, "banana-ui: gb companion: %s: not a release the companion knows (sha1 %s)\n",
							t.c_str(), ds.empty() ? "" : ds[0].game.romHash.c_str());
						l.unsupported = true;
						return l;
					}
					GameData& d = ds[k];
					std::string why;
					l.rom = readRom(rom, d, &why);
					l.d = d;
					if (!l.rom) fprintf(stderr, "banana-ui: gb companion: no game data: %s\n", why.c_str());
					else fprintf(stderr, "banana-ui: gb companion: %s (%s) read in %.0f ms\n", d.game.title.c_str(),
						d.releases[d.release].name.c_str(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
					if (l.rom) l.guide = guideFor(d, *l.rom);
					return l;
				});
			}
		}
		if (loading && load.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			loading = false;
			Loaded l = load.get();
			if (!l.rom)	// ("Not supported" for a release it doesn't know, else "No game data"; nothing asked of the game)
			{
				loadFailed = true;
				base.unsupported = l.unsupported;
				out = nds::PageData();
				out.unsupported = l.unsupported;
				fresh = true;
				return true;
			}
			begin(std::move(l));
		}
		if (!session) return fresh;
		// a new set of the game's memory: as Live to the Session (and the badges to the rules, for Gen II's boosts)
		if (reader.serial() != lastSerial)
		{
			lastSerial = reader.serial();
			auto l = reader.live(base.rom.get());
			if (l->valid)
			{
				setBadges(l->badges);
				if (pure) switches();
				if (teams) badgeTeams(l->badges);
				session->feed(l);
				fed++;
			}
		}
		// no save to show yet: why, when the emulator says (no memory map in this core)
		const std::string why = reader.error().empty() ? "" : "RetroArch can't read the game's memory with this emulator: " + reader.error();
		if (why != base.waitWhy) { base.waitWhy = why; out.waitWhy = why; fresh = true; }
		nds::SessionThread::Snapshot sn;
		if (session->take(sn))
		{
			out = base;
			out.live = sn.live;
			out.advice = sn.advice;
			out.journey = sn.journey;
			out.seenNotices = sn.seen;
			fresh = true;
		}
		return fresh;
	}

	// the ROM's data read: the rules, the reader (only now: nothing is asked of a game the companion can't read) and
	// the Session with the guide
	void Host::begin(Loaded l)
	{
		data = l.d;
		base.rom = l.rom;
		base.guide = l.guide ? l.guide : std::make_shared<nds::GuideData>();
		useRules(data.game.gen);
		fprintf(stderr, "banana-ui: gb companion: rules on (Generation %s)\n", data.gen == 1 ? "I" : "II");
		// the game's own rules, read from its ROM: its type chart, and PureRGB's (none for the tests' stand-in: rules.h
		// falls back on the generation's)
		asRead = base.rom;
		pure = false;
		switchesSeen = false;
		teams = false;
		lastTeam = 0;	// (the ROM's data as read: each trainer's first party)
		if (const auto* gb = dynamic_cast<const GbRomData*>(base.rom.get()); gb && gb->rom)
		{
			rules = std::make_unique<GameRules>(gb->rules);
			useGameRules(rules.get());
			pure = gb->rules.pureOptions || !gb->originalTypes.empty();
			teams = !gb->teams.empty();
		}
		setSwitches(0, false);
		reader.open(data);
		startSession();
		lastSerial = 0;
	}

	void Host::startSession()
	{
		session.reset();	// (the one before, stopped first: it saves the journey, which this one reads)
		auto ss = std::make_unique<nds::Session>(base.rom, base.guide, stateDir);
		ss->setChallenge(challenge);
		session = std::make_unique<nds::SessionThread>(std::move(ss));
		out = base;
		fresh = true;
	}

	// PureRGB's switches in the set just read: the options and the signature moves to the rules (each set, as the
	// badges); a change of the species given back their original types starts the Session again on a copy of the ROM's
	// data with those types (its pages and advice take a species' types from it), logged with the options
	void Host::switches()
	{
		const auto* gb = dynamic_cast<const GbRomData*>(asRead.get());
		const uint32_t remap = reader.typeRemap();
		const uint8_t options = reader.options3() & 15;
		const bool sigOff = reader.signaturesOff();
		setSwitches(options, sigOff);
		if (switchesSeen && remap == lastRemap && options == lastOptions && sigOff == lastSigOff) return;
		if (switchesSeen ? remap != lastRemap : remap != 0)
		{
			base.rom = withOriginalTypes(asRead, remap);
			startSession();
		}
		int n = 0;
		for (size_t i = 0; gb && i < gb->originalTypes.size() && i < 32; i++) n += remap >> i & 1;
		fprintf(stderr, "banana-ui: gb companion: PureRGB: %d species with their original types, matchups 0x%x%s\n", n,
			options, sigOff ? ", signature moves off" : "");
		switchesSeen = true;
		lastRemap = remap;
		lastOptions = options;
		lastSigOff = sigOff;
	}

	// Crystal Clear's trainers whose party goes by the badge count: a change of the party the save's badges give starts
	// the Session again on a copy of the ROM's data with each one's (its Battle page's foe and the guide's next fight
	// take a trainer's team from it), logged, and with a guide made from the ROM for these badges (its region first,
	// its leaders' levels as they'll be met: gsguide.h). (A game has these or PureRGB's switches, never both: the copy
	// is of the data as read)
	void Host::badgeTeams(uint16_t badges)
	{
		const int t = badgeTeam(badges);
		if (t == lastTeam) return;
		lastTeam = t;
		base.rom = withBadgeTeams(asRead, t);
		fprintf(stderr, "banana-ui: gb companion: the trainers' parties for %d badges\n", t);
		if (data.romGuide) base.guide = guideFor(data, *asRead, badges);
		startSession();
	}

	bool Host::takeData(nds::PageData& d)
	{
		if (!fresh) return false;
		d = out;
		fresh = false;
		return true;
	}

	void Host::markSeen(const std::vector<std::string>& ids)
	{
		if (session && !ids.empty()) session->markSeen(ids);
	}
}
