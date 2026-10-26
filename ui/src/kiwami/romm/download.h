#pragma once
// The RomM helper (kiwami): games and BIOS files from RomM onto the cards, the file work of romm download and romm
// bios (main.cpp has the verbs).
//   a game        into the target card's roms/<system's folder>/: one file as <fs_name>, one file in a folder of its
//                 own as <fs_name>/<its path>, several as RomM's zip unpacked into <fs_name>/. On its way it has a name
//                 starting with a dot, which the library doesn't list (library.cpp's listRoms skips them):
//                 .romm-<id>.part, or .romm-<id>.zip.part, and .romm-<id>.dir for a folder; renamed once whole. A .part
//                 left by a download cut short is resumed from where it stopped
//   a BIOS file   into <card>/bios/<file name>, through <file name>.part
// While either runs, bananad's idle sleep is held off by <run>/awake.pid, when that names no live process
// (banana-scrape writes it too).
#include "api.h"
#include "saves.h"
#include <functional>
#include <string>

namespace banana::kiwami::romm
{
	// the second card's roms folder while it's in (bananad's sd2 script names it in <run>/sd2/roms), else ""
	std::string sd2Roms(const Card& c);

	// where a game goes: romm_target's card, or the main card when that is the second card and it isn't in (fellBack)
	struct Target { std::string roms; bool fellBack = false; };
	Target target(const Card& c);
	// a system's folder under a roms folder (an absolute one is its own)
	std::string folderOn(const std::string& roms, const System& s);

	// a name RomM gives a file or a folder, safe to put on the card: not empty, no '/', not starting with '.'; and a
	// path in a game's folder, each of its parts safe
	bool safeName(const std::string& n);
	bool safePath(const std::string& p);

	// the free bytes where path is, or would be (its nearest folder that is there); KIWAMI_ROMM_FREE stands in for it
	// in the tests. -1: unknown
	long long freeBytes(const std::string& path);

	// awake.pid taken while this lives, when it names no live process; let go only if it still names this one
	struct Awake
	{
		explicit Awake(const Card& c);
		~Awake();
		std::string path;
		bool held = false;
	};

	// part filled with a file: from what it already has (get asked from that byte on), from the start when it has
	// nothing or more than expected, or when the server sends the whole file again. expected 0: unknown (a zip). ok
	// once it is whole; else status is the HTTP status (0: no answer) and why "down", "cut" (some of it came and is
	// kept for the next try), "notfound", "size" (whole, but not the size RomM gave; dropped) or "http <n>"
	struct Transfer { bool ok = false; long status = 0; std::string why; };
	Transfer transfer(const std::string& part, long long expected, const std::function<Response(long long from)>& get);

	// RomM's zip unpacked into dir (unzip, busybox's on the handheld)
	bool unpack(const std::string& zip, const std::string& dir, std::string& error);
	// a folder and all it holds removed (a .romm-<id>.dir an unpack left)
	bool removeTree(const std::string& dir);
	// a file's MD5, "" when it can't be read
	std::string fileMd5(const std::string& path);
}
