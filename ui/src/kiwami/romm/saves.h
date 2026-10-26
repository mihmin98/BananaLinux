#pragma once
// The RomM helper (kiwami): the games' saves on the card, where each emulator keeps them. File work only; the
// network is api.h's.
//   RetroArch's systems   <card>/saves/<core's library_name>/<rom stem>.srm (retroarch.cfg's savefile_directory with
//                         RetroArch's sort_savefiles_enable, on by default), the automatic state (Quick resume)
//                         <card>/states/<library_name>/<rom stem>.state.auto. The core is the one run-retroarch
//                         picks: settings.conf's core.<system> when it is installed, else systems.conf's
//   the DS                <card>/saves/nds/<rom stem>.sav, BananaDS's raw save, which nds-saves keeps in step with
//                         DraStic's <card>/drastic/backup/<rom stem>.dsv before it is read; sent to RomM as melonDS's
// A save the helper replaces is kept as <file>.bak first, and the new one goes through <file>.new and a rename.
#include "../rommcard.h"
#include <string>
#include <vector>

namespace banana::kiwami::romm
{
	// a section of systems.conf: the card's (<card>/banana/systems.conf) when there is one, else the image's
	struct System
	{
		std::string id, folder, run;
		std::vector<std::string> ext;
		bool flat = false;
	};
	std::vector<System> systems(const Card& c);
	std::vector<System> parseSystems(const std::string& text);
	const System* findSystem(const std::vector<System>& all, const std::string& id);

	// the systems whose saves are synced, with RomM's platform slugs for them (backend/utils/platform_slugs.py)
	struct Synced { const char* system; const char* slugs[2]; };
	extern const Synced SYNCED[9];
	const Synced* synced(const std::string& system);

	// the core that will run the system's games ("nds" for the DS, "" when the run line names none), and the folder
	// RetroArch keeps its saves in (nullptr for a core the helper doesn't know)
	std::string coreOf(const Card& c, const System& s);
	const char* libraryName(const std::string& core);

	struct Save
	{
		std::string system, rom, stem;
		std::string path;		// the save file
		std::string state;		// RetroArch's automatic state ("" for the DS)
		std::string dsv;		// DraStic's save (the DS only)
		std::string emulator;	// the core's id for RomM: the libretro core, melonds for the DS
		bool ds = false;
	};
	// the game's save and where it goes; false (why set) for a core the helper doesn't know
	bool saveOf(const Card& c, const System& s, const std::string& rom, Save& out, std::string& why);

	// nds-saves <rom>: DraStic's and BananaDS's saves made the same before one is read or after one is written
	void ndsSaves(const Card& c, const std::string& rom);

	// the save's bytes and modification time (seconds, and milliseconds); false when there is none
	bool readSave(const Save& s, std::string& bytes, long long& mtimeMs);

	// a save as RomM sent it: its bytes, or a zip another client made of them (grout's: the save, and the clock RetroArch
	// keeps beside it for a cartridge with one). RomM hashes a zip by what it holds (assets_handler.py's
	// hash_zip_contents), so hash is what RomM's content_hash is for what came
	struct Payload
	{
		std::string save, rtc;	// the save's bytes, and its clock's ("" for none)
		std::string hash;		// RomM's hash of what came
		bool zip = false;
	};
	// bytes that begin as a zip and unpack as one are a zip: one .srm, .sav or save.sram, at most one .rtc (another
	// clock.rtc, __MACOSX/, ._* and anything else ignored, logged); false, why "shape", for a zip without exactly one
	// save. Anything else is the save itself
	bool payloadOf(const Card& c, const std::string& bytes, Payload& out, std::string& why);
	// RomM's hash of a zip: md5 of "<name>:<md5>" lines, one per file, by name
	std::string zipHash(std::vector<std::pair<std::string, std::string>> files);

	// the save replaced by bytes, the old one kept as .bak (for the DS, written as DraStic's .dsv and then nds-saves
	// run, which makes BananaDS's .sav of it at its exact size, a DeSmuME footer left off); RetroArch's automatic state
	// renamed .state.auto.bak, so the game starts from this save and not from Quick resume's state. A clock (rtc, not
	// empty) goes beside a RetroArch save as <stem>.rtc, the old one kept as .bak too
	bool replaceSave(const Card& c, const Save& s, const std::string& bytes, std::string& error, const std::string& rtc = "");
	// on RomM's delete: the save renamed .bak (both of the DS's), nothing deleted
	bool dropSave(const Save& s, std::string& error);

	std::string md5Hex(const std::string& bytes);
	std::string stemOf(const std::string& path);	// the file's name without its folder and last extension
}
