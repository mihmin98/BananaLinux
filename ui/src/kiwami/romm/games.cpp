// The RomM helper (kiwami): a game on the card's RomM id (games.h).
#include "games.h"
#include "http.h"
#include "../conf.h"
#include <cstdlib>
#include <sstream>
#include <strings.h>
#include <unistd.h>

namespace banana::kiwami::romm
{
	namespace
	{
		std::string baseName(const std::string& p)
		{
			size_t s = p.rfind('/');
			return s == std::string::npos ? p : p.substr(s + 1);
		}

		// a field for a tab-separated line: tabs and line ends as spaces
		std::string field(std::string s)
		{
			for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
			return s;
		}

		std::vector<std::string> split(const std::string& ln)
		{
			std::vector<std::string> f;
			std::istringstream in(ln);
			std::string x;
			while (std::getline(in, x, '\t')) f.push_back(x);
			return f;
		}
	}

	std::vector<std::string> namesOf(const System& s, const std::string& rom)
	{
		std::vector<std::string> n = { baseName(rom) };
		size_t sl = rom.rfind('/');
		if (sl != std::string::npos && sl > 0)
		{
			std::string folder = baseName(rom.substr(0, sl));
			if (!folder.empty() && folder != baseName(s.folder)) n.push_back(folder);
		}
		return n;
	}

	long matchRom(const std::vector<Rom>& list, const std::string& name)
	{
		for (auto& r : list) if (r.fsName == name) return r.id;
		for (auto& r : list) if (r.fsName.size() == name.size() && strcasecmp(r.fsName.c_str(), name.c_str()) == 0) return r.id;
		return 0;
	}

	Games::Games(const Card& c, Server& s, bool r, std::function<bool(Server&)> b) : card(c), server(s), refetch(r), before(b) {}

	Games::~Games() { flush(); }

	bool Games::cached(const std::string& slug)
	{
		if (lists.count(slug)) return true;
		std::string text;
		bool there = false;
		if (!readFile(card.dir + "/platforms/" + slug + ".tsv", text, there) || !there) return false;
		std::vector<Rom>& l = lists[slug];
		std::istringstream in(text);
		std::string ln;
		while (std::getline(in, ln))
		{
			std::vector<std::string> f = split(ln);
			if (f.size() < 4) continue;
			Rom r;
			r.id = atol(f[0].c_str());
			r.fsName = f[1];
			r.size = atoll(f[2].c_str());
			r.name = f[3];
			if (r.id) l.push_back(r);
		}
		return true;
	}

	bool Games::fetch(const Synced& m)
	{
		if (!platformsFetched)
		{
			if (before && !before(server)) { down = true; return false; }
			Platforms p = platforms(server);
			status = p.status;
			if (!p.ok) { down = true; return false; }
			platformList = p.list;
			platformsFetched = true;
		}
		makeDir(card.dir + "/platforms");
		for (const char* slug : m.slugs)
		{
			if (!slug || fetched.count(slug)) continue;
			// every platform of that slug (a library can have two), 500 games a page; a slug RomM doesn't have is kept
			// as an empty list, so it isn't asked for again until a miss
			std::vector<Rom> all;
			for (auto& pl : platformList)
			{
				if (pl.slug != slug) continue;
				for (long offset = 0;;)
				{
					if (before && !before(server)) { down = true; return false; }
					Roms page = roms(server, pl.id, "", offset, 500);
					status = page.status;
					if (!page.ok) { down = true; return false; }
					all.insert(all.end(), page.items.begin(), page.items.end());
					offset += 500;
					if (page.items.empty() || offset >= page.total) break;
				}
			}
			std::string text, error;
			for (auto& r : all)
				text += std::to_string(r.id) + "\t" + field(r.fsName) + "\t" + std::to_string(r.size) + "\t" + field(r.name) + "\n";
			if (!replaceFile(card.dir + "/platforms/" + slug + ".tsv", text, error)) log("%s: %s", verb.c_str(), error.c_str());
			log("%s: RomM's %s list: %zu games", verb.c_str(), slug, all.size());
			lists[slug] = all;
			fetched.insert(slug);
		}
		return true;
	}

	long Games::lookUp(const Synced& m, const std::vector<std::string>& names)
	{
		for (auto& n : names)
			for (const char* slug : m.slugs)
				if (slug && cached(slug))
					if (long id = matchRom(lists[slug], n)) return id;
		return 0;
	}

	// the lines added into games, each replacing the line of its system and name there
	void Games::write(const std::vector<std::vector<std::string>>& add)
	{
		std::string text, error;
		bool there = false;
		readFile(card.dir + "/games", text, there);
		std::string out;
		std::istringstream in(text);
		std::string ln;
		while (std::getline(in, ln))
		{
			std::vector<std::string> f = split(ln);
			bool replaced = false;
			for (auto& a : add) replaced |= f.size() >= 2 && f[0] == a[0] && f[1] == a[1];
			if (!replaced) out += ln + "\n";
		}
		for (auto& a : add) out += a[0] + "\t" + a[1] + "\t" + a[2] + "\n";
		makeDir(card.dir);
		if (!replaceFile(card.dir + "/games", out, error)) log("%s: %s", verb.c_str(), error.c_str());
	}

	void Games::record(const std::string& system, const std::string& name, long id)
	{
		std::vector<std::string> line = { system, field(name), std::to_string(id) };
		for (auto it = records.begin(); it != records.end();)
			it = (*it)[0] == line[0] && (*it)[1] == line[1] ? records.erase(it) : it + 1;
		records.push_back(line);
		if (batch) unwritten.push_back(line);
		else write({ line });
	}

	void Games::flush()
	{
		if (unwritten.empty()) return;
		write(unwritten);
		unwritten.clear();
	}

	long Games::idOf(const System& s, const std::string& rom)
	{
		const Synced* m = synced(s.id);
		if (!m) return 0;
		std::vector<std::string> names = namesOf(s, rom);
		// the record, read once a run
		if (!recordsRead)
		{
			std::string text;
			bool there = false;
			readFile(card.dir + "/games", text, there);
			std::istringstream in(text);
			std::string ln;
			while (std::getline(in, ln))
			{
				std::vector<std::string> f = split(ln);
				if (f.size() >= 3) records.push_back({ f[0], f[1], f[2] });
			}
			recordsRead = true;
		}
		for (auto& n : names)
			for (auto& r : records)
				if (r[0] == s.id && r[1] == n) return atol(r[2].c_str());
		// the cached lists, fetched when none is
		bool any = false;
		for (const char* slug : m->slugs) any |= slug && cached(slug);
		if (!any && !fetch(*m)) return 0;
		long id = lookUp(*m, names);
		// a miss: the lists again, once a run, when this run may
		if (!id && refetch)
		{
			bool all = true;
			for (const char* slug : m->slugs) all &= !slug || fetched.count(slug) > 0;
			if (!all)
			{
				for (const char* slug : m->slugs) if (slug) lists.erase(slug);
				if (!fetch(*m)) return 0;
				id = lookUp(*m, names);
			}
		}
		if (id) record(s.id, names[0], id);
		else log("%s: %s: not on RomM (%s's list)", verb.c_str(), names[0].c_str(), m->slugs[0]);
		return id;
	}
}
