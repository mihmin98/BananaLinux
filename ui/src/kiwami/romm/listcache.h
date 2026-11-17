#pragma once
// banana-sync (kiwami): RomM's game list kept on the card (banana/kiwami-romm-list.jsonl) and brought up to date, so a
// sync of every system (Sync now, Get games' own run, the Wi-Fi's return with saves waiting) reads only what changed on
// the server instead of every game again. The file is a header line, {"server": <its address>, "version": 1,
// "listed_at": <the server's time the listing began>}, then each game as slim() keeps it (../syncmem.h), one a line,
// system by system in list()'s order and each system's in RomM's name order.
// In such a run, the first platform's Pages (lookup.h) reads the file once through (ListCache::begin()): when it is
// this server's, one GET /api/roms/identifiers (RomM 5.4.1's endpoints/roms/__init__.py get_rom_identifiers(): every
// game's id the token sees) says which are still there. Then each platform the file has (Cached) asks only for the
// games RomM changed since (updated_after: the server's own time of the last listing, its Date header, less 60 s; with
// none, the platform's newest updated_at less 60 s; RomM keeps a game updated after it, roms_handler.py
// filter_roms()), and list() gets its pages from the file, a line at a time, with the gone games left out and the
// changed and new ones merged in where RomM's name order puts them (name_sort_key, then id: roms_handler.py
// _resolve_gallery_sort_key(), _gallery_order_clauses()), so its loop reads them as RomM's own pages. A platform the
// file doesn't have is listed in full. Everything is listed in full when there is no file, or it is another server's,
// or identifiers can't be read, or for Get games' X (banana-sync games, sync.cpp's kiwami: line, fullListing()).
// After a listing that went through (record(), lookup.h), the file is written again (keepList(): to a .new, then
// renamed), unless nothing changed; a listing broken off leaves it as it was. Forget and a pairing with another server
// remove it (dropState()). One game's sync never reads nor writes it.
#include "romm.h"
#include "../syncmem.h"
#include "../synctimes.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <fstream>
#include <set>

namespace banana::kiwami::romm
{
	// a line for sync.log in a sync (its "running" status): `banana-sync games` prints its games on stdout, which say()
	// writes to too, so there it goes to stderr
	inline void tell(const char* fmt, ...)
	{
		char b[512];
		va_list a;
		va_start(a, fmt);
		vsnprintf(b, sizeof b, fmt, a);
		va_end(a);
		if (banana::kiwami::synctimes::lastStatus().compare(0, 8, "running ") == 0) bsync::say("%s", b);
		else fprintf(stderr, "%s\n", b);
	}

	// a JSON value's text (a slimmed game, for the file and for the pages made from it)
	inline void toJson(const JVal& v, std::string& o)
	{
		switch (v.kind)
		{
		case JVal::NUL: o += "null"; break;
		case JVal::BOOL: o += v.b ? "true" : "false"; break;
		case JVal::NUM:
		{
			char b[32];
			if (!std::isfinite(v.n)) snprintf(b, sizeof b, "null");
			else if (v.n == std::floor(v.n) && std::fabs(v.n) < 9e15) snprintf(b, sizeof b, "%lld", (long long)v.n);
			else snprintf(b, sizeof b, "%.17g", v.n);
			o += b;
			break;
		}
		case JVal::STR: o += bsync::jsonStr(v.s); break;
		case JVal::ARR:
			o += '[';
			for (size_t i = 0; i < v.arr.size(); i++) { if (i) o += ", "; if (v.arr[i]) toJson(*v.arr[i], o); else o += "null"; }
			o += ']';
			break;
		case JVal::OBJ:
			o += '{';
			for (size_t i = 0; i < v.obj.size(); i++)
			{
				if (i) o += ", ";
				o += bsync::jsonStr(v.obj[i].first) + ": ";
				if (v.obj[i].second) toJson(*v.obj[i].second, o); else o += "null";
			}
			o += '}';
			break;
		}
	}

	// RomM's time (ISO 8601: a fraction, and Z or an offset, or neither for UTC) as unix time; 0 when it can't be read
	inline long long isoTime(const std::string& s)
	{
		if (s.size() < 19) return 0;
		std::string d = s.substr(0, 19);
		d[10] = 'T';
		struct tm t = {};
		const char* e = strptime(d.c_str(), "%Y-%m-%dT%H:%M:%S", &t);
		if (!e || *e) return 0;
		long long v = (long long)timegm(&t);
		size_t i = 19;
		while (i < s.size() && (s[i] == '.' || isdigit((unsigned char)s[i]))) i++;
		if (i + 6 <= s.size() && (s[i] == '+' || s[i] == '-') && s[i + 3] == ':')
		{
			long long off = atoll(s.substr(i + 1, 2).c_str()) * 3600 + atoll(s.substr(i + 4, 2).c_str()) * 60;
			v += s[i] == '+' ? -off : off;
		}
		return v;
	}
	inline std::string isoUtc(long long t)
	{
		time_t tt = (time_t)t;
		struct tm g;
		gmtime_r(&tt, &g);
		char b[32];
		strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &g);
		return b;
	}
	// the server's own time when it answered (its Date header: "Tue, 17 Nov 2026 10:00:00 GMT"); 0 with none
	inline long long serverTime(const bsync::Resp& r)
	{
		auto h = r.hdr.find("date");
		if (h == r.hdr.end()) return 0;
		struct tm t = {};
		if (!strptime(h->second.c_str(), "%a, %d %b %Y %H:%M:%S", &t)) return 0;
		return (long long)timegm(&t);
	}
	// where RomM puts a game in its name order: its name_sort_key (the name lowercased, without a leading article, its
	// numbers padded), then its id; the name lowercased for a server that gives no key
	inline bool before(const JVal& a, const JVal& b)
	{
		auto key = [](const JVal& g) { std::string k = g.str("name_sort_key"); return k.empty() ? bsync::lower(g.str("name")) : k; };
		std::string ka = key(a), kb = key(b);
		return ka != kb ? ka < kb : a.num("id") < b.num("id");
	}

	// the file as this run found it, and what the listing does with it
	struct ListCache
	{
		bool full = false;		// every system listed in full (fullListing())
		bool begun = false;		// begin() done: once a run, by the first platform's Pages
		bool use = false;		// the file is this server's, and RomM's game ids were read
		bool changed = false;	// the listing differs from the file: written again at its end
		long long since = 0;	// the file's listed_at
		long long listedAt = 0;	// this listing's: the server's time of its first answer
		std::set<long long> ids;	// every game RomM has (identifiers)
		// a platform's lines in the file: from the first's offset to past the last, their games, the newest updated_at
		struct Span { std::streamoff from = -1, to = 0; std::set<long long> ids; long long newest = 0; };
		std::map<long long, Span> platforms;
		void saw(const bsync::Resp& r) { if (!listedAt && r.code) listedAt = serverTime(r); }
		void begin(const bsync::Backend& b)
		{
			begun = true;
			changed = true;
			std::ifstream f(listFile(), std::ios::binary);
			std::string head;
			if (full) { tell("  the games list: every system listed in full (Get games' refresh)"); return; }
			if (!f || !std::getline(f, head)) { tell("  the games list: every system listed in full, none kept on the card"); return; }
			JPtr h = banana::parseJson(head);
			if (!h || !h->isObj() || h->str("server") != b.c.server || (long long)h->num("version") != 1)
			{
				tell("  the games list: every system listed in full, the one kept on the card is another server's");
				return;
			}
			since = (long long)h->num("listed_at");
			bsync::Resp r = b.call("GET", "/api/roms/identifiers");
			saw(r);
			JPtr j = r.code == 200 ? r.json() : nullptr;
			if (!j || !j->isArr())
			{
				tell("  the games list: every system listed in full, RomM's game ids can't be read (%s)", r.code == 200 ? "not a list" : r.error().c_str());
				return;
			}
			for (auto& v : j->arr) if (v && v->isNum()) ids.insert((long long)v->n);
			std::streamoff at = (std::streamoff)head.size() + 1;
			for (std::string ln; std::getline(f, ln); at += (std::streamoff)ln.size() + 1)
			{
				JPtr it = banana::parseJson(ln);
				if (!it || !it->isObj()) continue;
				Span& s = platforms[(long long)it->num("platform_id")];
				if (s.from < 0) s.from = at;
				s.to = at + (std::streamoff)ln.size() + 1;
				s.ids.insert((long long)it->num("id"));
				s.newest = std::max(s.newest, isoTime(it->str("updated_at")));
			}
			use = true;
			changed = false;
		}
	};
	inline ListCache& listCache() { static ListCache c; return c; }
	// Get games' X (banana-sync games): everything listed again, the file written from it
	inline void fullListing() { listCache().full = true; }

	// one platform's pages from the file, brought up to date (Pages, for a platform the file has)
	struct Cached
	{
		const bsync::Backend& b;
		const long long pid;
		const std::string folder;
		std::ifstream in;
		std::streamoff at = 0, end = 0;
		std::vector<JPtr> fresh;	// the platform's games changed or new on the server since, in RomM's name order
		size_t next = 0;
		std::set<long long> freshIds;
		JPtr kept;					// the next game of the file to go, and its line
		std::string keptText;
		long long total = 0;
		Cached(const bsync::Backend& backend, long long pid_, const std::string& folder_) : b(backend), pid(pid_), folder(folder_) {}
		// the games RomM changed since the last listing (their pages read here: few, but as many as there are); a
		// failed answer is list()'s to say, as a page's would be
		bsync::Resp changes()
		{
			ListCache& lc = listCache();
			const ListCache::Span& sp = lc.platforms[pid];
			long long since = std::max(0LL, (lc.since > 0 ? lc.since : sp.newest) - 60);
			const std::string q = "&updated_after=" + bsync::urlEsc(isoUtc(since));
			for (long long off = 0;; off += banana::kiwami::syncmem::PAGE)
			{
				bsync::Resp r = b.call("GET", romsQuery(pid, off, banana::kiwami::syncmem::PAGE, q));
				if (r.code != 200) return r;
				JPtr page = r.json();
				const JVal* items = page ? (page->isArr() ? page.get() : page->get("items")) : nullptr;
				if (!items || !items->isArr()) return r;
				for (auto& it : items->arr)
					if (it && it->isObj() && (long long)it->num("platform_id") == pid && freshIds.insert((long long)it->num("id")).second)
						fresh.push_back(banana::kiwami::syncmem::slim(*it));
				long long n = page->isArr() ? -1 : (long long)page->num("total", -1);
				if (page->isArr() || items->arr.size() < banana::kiwami::syncmem::PAGE || (n >= 0 && off + banana::kiwami::syncmem::PAGE >= n)) break;
			}
			std::sort(fresh.begin(), fresh.end(), [](const JPtr& x, const JPtr& y) { return before(*x, *y); });
			long long keep = 0, gone = 0;
			for (long long id : sp.ids)
			{
				if (!lc.ids.count(id)) gone++;
				else if (!freshIds.count(id)) keep++;
			}
			total = keep + (long long)fresh.size();
			if (gone || !fresh.empty()) lc.changed = true;
			in.open(listFile(), std::ios::binary);
			at = sp.from;
			end = sp.to;
			if (in) in.seekg(at);
			tell("  %s: %lld games as kept on the card, %zu changed or new on the server since %s, %lld gone", folder.c_str(), keep, fresh.size(), isoUtc(since).c_str(), gone);
			bsync::Resp ok;
			ok.code = 200;
			return ok;
		}
		// the next PAGE games: the file's that are still there and unchanged, and the changed ones, by RomM's name order
		bsync::Resp page()
		{
			std::string body = "{\"items\": [";
			size_t n = 0;
			while (n < (size_t)banana::kiwami::syncmem::PAGE)
			{
				if (!kept) readKept();
				bool f = next < fresh.size();
				if (!kept && !f) break;
				if (n++) body += ", ";
				if (f && (!kept || before(*fresh[next], *kept))) { toJson(*fresh[next], body); fresh[next++].reset(); }
				else { body += keptText; kept.reset(); }
			}
			bsync::Resp r;
			r.code = 200;
			r.body = body + "], \"total\": " + std::to_string(total) + "}";
			return r;
		}

	private:
		void readKept()
		{
			const ListCache& lc = listCache();
			std::string ln;
			while (in && at < end && std::getline(in, ln))
			{
				at += (std::streamoff)ln.size() + 1;
				JPtr it = banana::parseJson(ln);
				if (!it || !it->isObj() || (long long)it->num("platform_id") != pid) continue;
				long long id = (long long)it->num("id");
				if (!lc.ids.count(id) || freshIds.count(id)) continue;
				kept = it;
				keptText = ln;
				return;
			}
		}
	};

	// after a listing of every system (record(); R: sync_romm.cpp's Romm, its c and romJson): the file written again
	// from it when it differs from the file
	template <class R> void keepList(const R& r)
	{
		ListCache& lc = listCache();
		if (!lc.begun || !lc.changed) return;
		const std::string tmp = listFile() + ".new." + std::to_string((long long)getpid());
		FILE* w = fopen(tmp.c_str(), "w");
		if (!w) { tell("  the games list couldn't be kept on the card (%s)", tmp.c_str()); return; }
		std::string line = "{\"server\": " + bsync::jsonStr(r.c.server) + ", \"version\": 1, \"listed_at\": " + std::to_string(lc.listedAt) + "}\n";
		bool ok = fputs(line.c_str(), w) >= 0;
		for (auto& j : r.romJson)
		{
			line.clear();
			toJson(*j, line);
			line += '\n';
			ok = ok && fwrite(line.data(), 1, line.size(), w) == line.size();
		}
		ok = fflush(w) == 0 && fsync(fileno(w)) == 0 && ok;
		ok = fclose(w) == 0 && ok;
		if (!ok || rename(tmp.c_str(), listFile().c_str()) != 0)
		{
			unlink(tmp.c_str());
			tell("  the games list couldn't be kept on the card (%s)", listFile().c_str());
			return;
		}
		tell("  the games list kept on the card: %zu games", r.romJson.size());
	}
}
