#pragma once
// The RomM helper (kiwami): RomM's requests and their replies, as RomM 5.4.1 (tag 9c74d01) answers them: the device
// flow (backend/endpoints/device/auth.py), the devices (endpoints/device/__init__.py) and the heartbeat. The parsers
// take the body and the HTTP status (0: no answer) and need no network, so the unit test runs them.
#include <string>

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

	std::string jsonString(const std::string& s);	// s as a JSON string, quoted
}
