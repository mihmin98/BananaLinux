// banana-ui (kiwami): a game's RetroAchievements hash (rahash.h), through rcheevos' rc_hash_generate with an iterator
// of its own (no global state: the pages hash on their thread), its default file and disc readers.
#include "rahash.h"
#include "raclient.h"
#include <cstdio>
// as kiwami.mk builds rcheevos: they change rc_hash_iterator_t's layout, so both sides must agree
#define RC_HASH_NO_ENCRYPTED
#define RC_HASH_NO_ZIP
#include "third_party/rcheevos/include/rc_hash.h"

namespace banana::kiwami::ra
{
	namespace
	{
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
	}

	unsigned consoleOf(const std::string& system)
	{
		for (auto& c : CONSOLES) if (system == c.system) return c.id;
		return 0;
	}

	Hash hash(const std::string& path, const std::string& system)
	{
		Hash r;
		unsigned console = consoleOf(system);
		std::string e = extension(path);
		if (!console) { r.error = "no achievements for this system here"; return r; }
		if (e == "zip" || e == "7z") { r.error = "a game in a ." + e + " can't be identified yet"; return r; }
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
		rc_hash_iterator_t it;
		rc_hash_initialize_iterator(&it, path.c_str(), nullptr, 0);
		it.userdata = &r.error;
		it.callbacks.verbose_message = onMessage;
		it.callbacks.error_message = onError;
		char h[33] = "";
		bool ok = rc_hash_generate(h, console, &it) != 0;
		rc_hash_destroy_iterator(&it);
		if (ok) { r.hash = h; r.error.clear(); }
		else if (r.error.empty()) r.error = "it can't be read";
		return r;
	}
}
