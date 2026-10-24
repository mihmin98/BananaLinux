#pragma once
// banana-ui (kiwami): a game's RetroAchievements hash, as RetroArch and the DS emulators compute it, for the server's
// gameid (raclient.h). The hashing is rcheevos' own (third_party/rcheevos, built by kiwami.mk), so the IDs match: the
// whole cartridge for most systems (an NES's header and a SNES's copier header left out, an N64 in any byte order), a
// DS game's header, code and icon, a PlayStation disc's boot executable (.cue/.bin, .iso, .chd, an .m3u naming one). A
// cartridge in a .zip or .7z is hashed on the game in it, unpacked into memory, as RetroArch does. It blocks while it
// reads (up to a 64 MB cartridge): the pages run it on their thread. rcheevos' messages go to stderr as
// "banana-ui: ra: hash: ..." lines.
#include <string>
#include <vector>

namespace banana::kiwami::ra
{
	struct Hash { std::string hash, error; };	// hash: 32 hex digits, "" with error saying why there is none

	// the game at path on one of the RetroAchievements pages' systems (Banana's system folder: nds, gba, psx...); in an
	// archive, the game is the first file in name order whose extension is one of exts, the system's (SystemDef::exts)
	Hash hash(const std::string& path, const std::string& system, const std::vector<std::string>& exts = {});
	// RetroAchievements' console for the system, 0 for one the pages don't know
	unsigned consoleOf(const std::string& system);
}
