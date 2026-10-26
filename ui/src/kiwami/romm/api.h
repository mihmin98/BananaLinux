#pragma once
// The RomM helper (kiwami): RomM's requests and their replies, as RomM 5.4.1 (tag 9c74d01) answers them: the device
// flow (backend/endpoints/device/auth.py), the devices (endpoints/device/__init__.py), the heartbeat, the platforms and
// their games (endpoints/platform.py, endpoints/roms/), and the saves' sync (endpoints/sync/__init__.py,
// endpoints/saves.py), the games' and the firmware's files (endpoints/roms/__init__.py's get_rom_content,
// endpoints/firmware.py). The parsers take the body and the HTTP status (0: no answer) and need no network, so the unit
// test runs them.
#include "http.h"
#include <string>
#include <vector>

namespace banana::kiwami::romm
{
	// the server and the token every request after the pairing sends; the timeouts of its requests
	struct Server
	{
		std::string url, token;
		long connectMs = 5000, totalMs = 20000;
	};

	// the server's version against the one the client is written for: below 5.4.0 refused, 5.4.x, or newer (one
	// that isn't a version, a "development" build, counts as newer)
	enum Version { OLD, OK, NEWER };
	Version versionOk(const std::string& v);

	struct Heartbeat { bool ok = false; std::string version, error; };
	Heartbeat parseHeartbeat(const std::string& body, long status);
	Heartbeat heartbeat(const Server& s);

	// the pairing's start: the codes, the page the user approves it on (verification_path_complete, joined with the
	// server's address), how long it waits and how often to ask
	struct DeviceInit { bool ok = false; std::string deviceCode, userCode, path, error; int expiresIn = 600, interval = 5; };
	DeviceInit parseDeviceInit(const std::string& body, long status);
	// RomM's scopes the handheld asks for
	extern const char* const SCOPES[8];
	DeviceInit deviceInit(const Server& s, const std::string& name, const std::string& identifier, const std::string& version);

	// a poll of the pairing: the token and the device once approved, else where it stands
	struct DeviceToken
	{
		enum State { GRANTED, PENDING, SLOW_DOWN, DENIED, EXPIRED, FAILED } state = FAILED;
		std::string token, deviceId, error;
	};
	DeviceToken parseDeviceToken(const std::string& body, long status);
	DeviceToken deviceToken(const Server& s, const std::string& deviceCode);

	// the handheld's device: the HTTP status (200 there, 401 a token RomM no longer takes, 404 gone; 0 no answer)
	long getDevice(const Server& s, const std::string& id);
	long deleteDevice(const Server& s, const std::string& id);

	// ---- the library ----------------------------------------------------------------------------------------------
	struct Platform { long id = 0, romCount = 0; std::string slug, name; };
	struct Platforms { bool ok = false; long status = 0; std::vector<Platform> list; };
	Platforms parsePlatforms(const std::string& body, long status);
	Platforms platforms(const Server& s);

	// a page of a platform's games (search "" for all), in order of id or name: what the helper keeps of each. A game
	// is one file (simple: fs_name is the file's name), or a folder named fs_name holding one file or several; files,
	// their paths in that folder, come only with a game asked for alone (romDetail)
	struct RomFile { std::string path; long long size = 0; };
	struct Rom
	{
		long id = 0;
		long long size = 0;
		std::string fsName, name, platformSlug;
		bool simple = true;
		std::vector<RomFile> files;
	};
	struct Roms { bool ok = false; long status = 0, total = 0; std::vector<Rom> items; };
	Roms parseRoms(const std::string& body, long status);
	Roms roms(const Server& s, long platform, const std::string& search, long offset, long limit, const char* order = "id");
	struct RomDetail { bool ok = false; long status = 0; Rom rom; };
	RomDetail parseRomDetail(const std::string& body, long status);
	RomDetail romDetail(const Server& s, long id);

	// a platform's firmware (RomM's BIOS files)
	struct Firmware { long id = 0, platformId = 0; long long size = 0; std::string fileName, md5; };
	struct Firmwares { bool ok = false; long status = 0; std::vector<Firmware> list; };
	Firmwares parseFirmware(const std::string& body, long status);
	Firmwares firmware(const Server& s, long platform);

	// a file's bytes into file: a game's (one file as itself, several as a zip) or a firmware's. From resume on with a
	// Range when it is more than 0 (206 appended, 200 the whole file again); given up after 30 s under 1 KB a second,
	// with no limit on the whole. A zip of several files is always asked for with a Range, from 0 too: RomM then sends
	// its cached zip, the same bytes each time, which a download cut short can go on with (without one, nginx's mod_zip
	// builds another, which it can't)
	Response romContent(const Server& s, const Rom& r, const std::string& file, long long resume,
		const std::function<void(long long, long long)>& progress);
	Response firmwareContent(const Server& s, const Firmware& f, const std::string& file, long long resume,
		const std::function<void(long long, long long)>& progress);

	// ---- the saves' sync ------------------------------------------------------------------------------------------
	// a save on the card as the negotiation takes it; always in the slot "autosave"
	struct ClientSave { long romId = 0; std::string fileName, emulator, hash; long long mtimeMs = 0, size = 0; };
	// what RomM says to do with a save: upload, download, conflict, delete or no_op
	struct Operation { std::string action, slot, fileName, emulator, serverHash, serverTime; long romId = 0, saveId = 0; };
	struct Negotiation { bool ok = false; long status = 0, session = 0; std::vector<Operation> ops; std::string error; };
	Negotiation parseNegotiation(const std::string& body, long status);
	// rom_ids scopes it to the games given (at most 500); emulators is left out, so the newest autosave pairs whichever
	// emulator wrote it (RomM's web player's included)
	Negotiation negotiate(const Server& s, const std::string& deviceId, const std::vector<ClientSave>& saves, const std::vector<long>& romIds);

	// an upload: 409 is RomM saying the slot has a newer save since this device's last sync (a conflict)
	struct Upload { bool ok = false, conflict = false; long status = 0, saveId = 0; };
	Upload uploadSave(const Server& s, const ClientSave& save, const std::string& deviceId, long session, bool overwrite,
		const std::string& bytes);
	// a save's bytes (status 200), not yet counted as the device's (optimistic=false); then RomM told the device has it,
	// with RomM's own hash of it, which is the only one RomM takes as proof (confirm_download)
	long downloadSave(const Server& s, long id, const std::string& deviceId, long session, std::string& bytes);
	long confirmDownload(const Server& s, long id, const std::string& deviceId, const std::string& hash);
	// the session closed, with the plays it carries (seconds since 1970; one shorter than a second isn't sent, RomM
	// refuses it)
	struct PlaySession { long romId = 0; long long start = 0, end = 0; };
	long complete(const Server& s, long session, int done, int failed, const std::vector<PlaySession>& plays);
	// the plays alone (POST /api/play-sessions, 201), when no session carries them; RomM skips one it has already (the
	// same game and start)
	long playSessions(const Server& s, const std::string& deviceId, const std::vector<PlaySession>& plays);

	std::string jsonString(const std::string& s);	// s as a JSON string, quoted
	std::string isoTime(long long ms);				// UTC, 2026-10-24T12:00:00.000Z
	std::string urlEscape(const std::string& s);
}
