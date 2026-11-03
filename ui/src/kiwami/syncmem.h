#pragma once
// banana-sync (kiwami): RomM's game list held to the fields Romm::manifest() reads (ui/src/sync.cpp). Romm::list()
// asks for PAGE games at a time and keeps each as slim()'s copy: a small object with only the members manifest()
// reads, their values the same, so upstream's manifest() builds the same Manifest from it. The rest of each page (every
// provider's metadata, the RetroAchievements block with all its achievements: about 160 KB a game once parsed) is
// freed before the next request, where upstream kept every page's whole tree to the end and a large library outgrew
// the handheld's RAM. Header-only: banana-sync is built from sync.cpp and json.cpp alone.
// A key manifest() starts reading has to be added to the lists below, or slim() drops it and the card's games and
// gamelists change; kiwami/tests/romm-list.sh stops on one that isn't there.
#include "../json.h"
#include <memory>

namespace banana::kiwami::syncmem
{
	constexpr int PAGE = 50;	// RomM's games asked for this many at a time (upstream: 500)
	// the members of a game Romm::manifest() reads as scalars, of each of its files[], and of its metadatum
	inline const char* const GAME_KEYS[] = { "id", "platform_id", "fs_name", "name", "fs_size_bytes",
		"has_multiple_files", "missing_from_fs", "summary", "path_cover_large" };
	inline const char* const FILE_KEYS[] = { "id", "file_name", "file_size_bytes", "sha1_hash", "md5_hash" };
	inline const char* const META_KEYS[] = { "first_release_date" };
	inline const char* const META_FIRST[] = { "genres", "publishers" };	// only their first element is read

	namespace detail
	{
		inline std::shared_ptr<JVal> make(JVal::Kind kind)
		{
			auto v = std::make_shared<JVal>();
			v->kind = kind;
			return v;
		}
		// a member as JVal::get() finds it: the first of its name, none when o isn't an object
		inline JPtr member(const JVal& o, const char* key)
		{
			if (!o.isObj()) return nullptr;
			for (auto& m : o.obj) if (m.first == key) return m.second;
			return nullptr;
		}
		// a scalar member, when there is one: its own node for a string, number, boolean or null (none of them holds
		// anything of the page), a fresh null in place of an array or object. It stays present, as manifest() tests
		// files[0]'s file_size_bytes for presence, and a null answers str/num/flag with their defaults as an array or
		// object does
		inline void scalar(JVal& to, const JVal& from, const char* key)
		{
			JPtr v = member(from, key);
			if (!v) return;
			to.obj.emplace_back(key, v->isArr() || v->isObj() ? make(JVal::NUL) : v);
		}
	}

	// the game's copy holding only what manifest() reads; a game that isn't an object gives an empty object
	inline JPtr slim(const JVal& game)
	{
		auto out = detail::make(JVal::OBJ);
		if (!game.isObj()) return out;
		for (const char* k : GAME_KEYS) detail::scalar(*out, game, k);
		// files[]: as many as there were (manifest() counts them), each with its five fields; one that isn't an
		// object an empty object. files that isn't an array is left out (manifest() checks isArr())
		JPtr files = detail::member(game, "files");
		if (files && files->isArr())
		{
			auto fs = detail::make(JVal::ARR);
			for (auto& f : files->arr)
			{
				auto o = detail::make(JVal::OBJ);
				if (f) for (const char* k : FILE_KEYS) detail::scalar(*o, *f, k);
				fs->arr.push_back(o);
			}
			out->obj.emplace_back("files", fs);
		}
		// metadatum: its release date, and of genres and publishers (when arrays) the first element only, a string's
		// node or else a fresh null (manifest() reads arr[0]->s, "" for anything but a string). A metadatum that
		// isn't an object is left out (get() on it finds nothing)
		JPtr md = detail::member(game, "metadatum");
		if (md && md->isObj())
		{
			auto o = detail::make(JVal::OBJ);
			for (const char* k : META_KEYS) detail::scalar(*o, *md, k);
			for (const char* k : META_FIRST)
			{
				JPtr a = detail::member(*md, k);
				if (!a || !a->isArr()) continue;
				auto first = detail::make(JVal::ARR);
				if (!a->arr.empty()) first->arr.push_back(a->arr[0] && a->arr[0]->isStr() ? a->arr[0] : detail::make(JVal::NUL));
				o->obj.emplace_back(k, first);
			}
			out->obj.emplace_back("metadatum", o);
		}
		return out;
	}
}
