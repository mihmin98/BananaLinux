#pragma once
// banana-ui (kiwami): key=value files on the card (accounts.conf, settings.conf, retroarch.cfg), read and edited in
// place, every other line kept: RetroAchievements' account store (rastore.cpp), the RomM helper and its pages
// (rommcard.cpp). File work only; nothing here logs, the callers say what failed.
#include <string>
#include <vector>

namespace banana::kiwami
{
	// the file's value for key as upstream's share script reads it (spaces round the key and the value, quotes, the
	// last line winning), "" when there is none
	std::string confGet(const std::string& path, const std::string& key);

	// the whole file into s; false when it is there but can't be read (a card error), or (with s "") isn't there
	bool readFile(const std::string& path, std::string& s, bool& there);

	// the file replaced by text: through <path>.new and a rename, so a full card or an error keeps the old one
	bool replaceFile(const std::string& path, const std::string& text, std::string& error);

	// each key set (its first line replaced, the others dropped, or a line added at the end) or, with del, every line
	// of it dropped; every other line kept in its order. A file that isn't there is made only when create; one that
	// doesn't change isn't written
	struct ConfEdit { std::string key, value; bool del = false; };
	bool confEdit(const std::string& path, const std::vector<ConfEdit>& edits, const char* sep, bool create, std::string& error);
}
