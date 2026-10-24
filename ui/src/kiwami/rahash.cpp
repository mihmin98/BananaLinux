// banana-ui (kiwami): a game's RetroAchievements hash (rahash.h), through rcheevos' rc_hash_generate with an iterator
// of its own (no global state: the pages hash on their thread), its default file and disc readers. A .zip's game is
// read through upstream's readZip, a .7z's through ra7z, into a buffer rcheevos hashes.
#include "rahash.h"
#include "raclient.h"
#include "ra7z.h"
#include "../zip.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
// as kiwami.mk builds rcheevos: they change rc_hash_iterator_t's layout, so both sides must agree
#define RC_HASH_NO_ENCRYPTED
#define RC_HASH_NO_ZIP
#include "third_party/rcheevos/include/rc_hash.h"

namespace banana::kiwami::ra
{
	namespace
	{
		const uint64_t MAX_UNPACKED = 64ull << 20;	// the largest cartridge, an N64's

		// rcheevos' messages: each logged, the first error kept for the page
		void onMessage(const char* m, const rc_hash_iterator_t* it)
		{
			fprintf(stderr, "banana-ui: ra: hash: %s\n", m);
		}
		void onError(const char* m, const rc_hash_iterator_t* it)
		{
			onMessage(m, it);
			std::string* e = (std::string*)it->userdata;
			if (e && e->empty()) *e = m;
		}

		std::string extension(const std::string& path)
		{
			size_t slash = path.rfind('/'), dot = path.rfind('.');
			if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
			std::string e = path.substr(dot + 1);
			for (auto& c : e) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
			return e;
		}

		// names compared as RetroArch sorts an archive's files (libretro-common's dir_list_casecmp)
		bool before(const std::string& a, const std::string& b)
		{
			for (size_t i = 0;; i++)
			{
				int x = i < a.size() ? tolower((unsigned char)a[i]) : 0, y = i < b.size() ? tolower((unsigned char)b[i]) : 0;
				if (x != y || !x) return x < y;
			}
		}

		// The game in an archive, as RetroArch picks it (tasks/task_content.c's content_file_get_path): of the files
		// whose extension is the system's, archives aside, the first in name order. -1 for none.
		int pickGame(const std::vector<rnds::ZipMember>& files, const std::vector<std::string>& exts)
		{
			int best = -1;
			for (int i = 0; i < (int)files.size(); i++)
			{
				const std::string& n = files[i].name;
				std::string e = extension(n);
				if (n.empty() || n.back() == '/' || e.empty() || e == "zip" || e == "7z") continue;
				if (std::find(exts.begin(), exts.end(), e) == exts.end()) continue;
				if (best < 0 || before(n, files[best].name)) best = i;
			}
			return best;
		}

		// the hash of the file at path, or of data when it's given (path then only names it)
		void generate(Hash& r, unsigned console, const std::string& path, const uint8_t* data, size_t size)
		{
			rc_hash_iterator_t it;
			rc_hash_initialize_iterator(&it, path.c_str(), data, size);
			it.userdata = &r.error;
			it.callbacks.verbose_message = onMessage;
			it.callbacks.error_message = onError;
			char h[33] = "";
			bool ok = rc_hash_generate(h, console, &it) != 0;
			rc_hash_destroy_iterator(&it);
			if (ok) { r.hash = h; r.error.clear(); }
			else if (r.error.empty()) r.error = "it can't be read";
		}

		// a cartridge in a .zip or .7z: its game unpacked into memory, then hashed
		void generateFromArchive(Hash& r, unsigned console, const std::string& path, const std::string& e,
			const std::vector<std::string>& exts)
		{
			std::string member;
			auto pick = [&](const std::vector<rnds::ZipMember>& files)
			{
				int k = pickGame(files, exts);
				if (k >= 0) member = files[k].name;
				fprintf(stderr, "banana-ui: ra: hash: %s in %s\n", k >= 0 ? member.c_str() : "no game", path.c_str());
				return k;
			};
			Unpack u;
			if (e == "7z")
				u = read7z(path, pick, MAX_UNPACKED, [&](const uint8_t* p, size_t n) { generate(r, console, member, p, n); });
			else
			{
				// readZip lists the files, then streams the one picked: its size, from the archive's directory, is
				// checked before anything is read, and a member that turns out longer or shorter is damaged
				bool listed = false, big = false, wrong = false;
				uint64_t want = 0;
				std::vector<uint8_t> data;
				bool read = readZip(path, [&](const std::vector<rnds::ZipMember>& files)
				{
					listed = true;
					int k = pick(files);
					if (k >= 0 && files[k].size > MAX_UNPACKED) { big = true; return -1; }
					if (k >= 0) { want = files[k].size; data.reserve(want); }
					return k;
				}, [&](const uint8_t* p, size_t n)
				{
					if (data.size() + n > want) { wrong = true; return false; }
					data.insert(data.end(), p, p + n);
					return true;
				});
				u = !listed ? Unpack::DAMAGED : big ? Unpack::TOO_BIG : member.empty() ? Unpack::NONE
					: !read || wrong || data.size() != want ? Unpack::DAMAGED : Unpack::OK;
				if (u == Unpack::OK) generate(r, console, member, want ? data.data() : (const uint8_t*)"", data.size());
			}
			if (u == Unpack::OK) return;
			r.error = u == Unpack::NONE ? "no game in the archive" : u == Unpack::TOO_BIG ? "over 64 MB unpacked" : "damaged archive";
		}
	}

	unsigned consoleOf(const std::string& system)
	{
		for (auto& c : CONSOLES) if (system == c.system) return c.id;
		return 0;
	}

	Hash hash(const std::string& path, const std::string& system, const std::vector<std::string>& exts)
	{
		Hash r;
		unsigned console = consoleOf(system);
		std::string e = extension(path);
		if (!console) { r.error = "no achievements for this system here"; return r; }
		if (e == "chd") { r.error = "a .chd can't be identified yet"; return r; }
		if (console == 12 && e == "pbp")
		{
			// (rcheevos reads a .pbp only for the PSP: RetroArch can't identify a PlayStation one either)
			r.error = "a PlayStation .pbp can't be identified, by RetroArch either";
			return r;
		}
		if (console == 12 && (e == "img" || e == "mdf"))
		{
			r.error = "a PlayStation ." + e + " can't be identified: as .chd or .cue and .bin it can";
			return r;
		}
		if (e == "zip" || e == "7z")
		{
			// (a DS game, 64 to 512 MB, would have to be unpacked whole: as .nds it is read where its header points)
			if (console == 18) r.error = "a DS game can't be identified from a ." + e;
			else generateFromArchive(r, console, path, e, exts);
			return r;
		}
		generate(r, console, path, nullptr, 0);
		return r;
	}
}
