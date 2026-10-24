// The RomM helper (kiwami): RomM's requests and their replies (api.h).
#include "api.h"
#include "http.h"
#include "../json.h"
#include <cstdio>
#include <cstdlib>

namespace banana::kiwami::romm
{
	const char* const SCOPES[8] = { "roms.read", "platforms.read", "firmware.read", "assets.read", "assets.write",
		"devices.read", "devices.write", "roms.user.write" };

	namespace
	{
		// FastAPI's error, {"detail": "..."}: the text, "" when the body isn't one
		std::string detail(const std::string& body)
		{
			json::Value v;
			return json::parse(body, v) ? v["detail"].str() : "";
		}

		std::string unreadable(long status)
		{
			return !status ? "no connection" : "unreadable answer (" + std::to_string(status) + ")";
		}

		int number(const json::Value& v, int dflt)
		{
			return v.type == json::Value::NUM && v.n > 0 ? (int)v.n : dflt;
		}
	}

	Version versionOk(const std::string& v)
	{
		const char* p = v.c_str();
		if (*p == 'v' || *p == 'V') p++;
		char* e = nullptr;
		long major = strtol(p, &e, 10);
		if (e == p || *e != '.') return NEWER;
		const char* q = e + 1;
		long minor = strtol(q, &e, 10);
		if (e == q) return NEWER;
		if (major != 5) return major < 5 ? OLD : NEWER;
		return minor < 4 ? OLD : minor == 4 ? OK : NEWER;
	}

	std::string jsonString(const std::string& s)
	{
		std::string o = "\"";
		for (unsigned char c : s)
			if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
			else if (c == '\n') o += "\\n";
			else if (c == '\r') o += "\\r";
			else if (c == '\t') o += "\\t";
			else if (c < 0x20) { char u[8]; snprintf(u, sizeof u, "\\u%04x", c); o += u; }
			else o += (char)c;
		return o + "\"";
	}

	Heartbeat parseHeartbeat(const std::string& body, long status)
	{
		Heartbeat h;
		json::Value v;
		if (status != 200 || !json::parse(body, v)) { h.error = unreadable(status); return h; }
		h.version = v["SYSTEM"]["VERSION"].str();
		if (h.version.empty()) { h.error = "not RomM's heartbeat"; return h; }
		h.ok = true;
		return h;
	}

	Heartbeat heartbeat(const Server& s)
	{
		Request r;
		r.url = s.url + "/api/heartbeat";
		r.connectMs = s.connectMs;
		r.totalMs = s.totalMs;
		Response res = perform(r);
		return parseHeartbeat(res.body, res.status);
	}

	DeviceInit parseDeviceInit(const std::string& body, long status)
	{
		DeviceInit d;
		json::Value v;
		if ((status != 200 && status != 201) || !json::parse(body, v))
		{
			std::string why = status ? detail(body) : "";
			d.error = why.empty() ? unreadable(status) : why + " (" + std::to_string(status) + ")";
			return d;
		}
		d.deviceCode = v["device_code"].str();
		d.userCode = v["user_code"].str();
		d.path = v["verification_path_complete"].str();
		if (d.path.empty()) d.path = v["verification_path"].str();
		d.expiresIn = number(v["expires_in"], 600);
		d.interval = number(v["interval"], 5);
		if (d.deviceCode.empty() || d.userCode.empty() || d.path.empty()) { d.error = "unreadable answer (no codes)"; return d; }
		d.ok = true;
		return d;
	}

	DeviceInit deviceInit(const Server& s, const std::string& name, const std::string& identifier, const std::string& version)
	{
		Request r;
		r.method = "POST";
		r.url = s.url + "/api/auth/device/init";
		r.type = "application/json";
		r.connectMs = s.connectMs;
		r.totalMs = s.totalMs;
		std::string scopes;
		for (const char* sc : SCOPES) scopes += (scopes.empty() ? "" : ", ") + jsonString(sc);
		r.body = "{\"client_device_identifier\": " + jsonString(identifier) + ", \"name\": " + jsonString(name)
			+ ", \"client\": \"BananaOS\", \"client_version\": " + (version.empty() ? "null" : jsonString(version))
			+ ", \"requested_scopes\": [" + scopes + "]}";
		Response res = perform(r);
		return parseDeviceInit(res.body, res.status);
	}

	DeviceToken parseDeviceToken(const std::string& body, long status)
	{
		DeviceToken t;
		json::Value v;
		bool parsed = status && json::parse(body, v);
		if (status == 200 && parsed)
		{
			t.token = v["access_token"].str();
			t.deviceId = v["device_id"].str();
			if (!t.token.empty() && !t.deviceId.empty()) { t.state = DeviceToken::GRANTED; return t; }
			t.error = "unreadable answer (no token)";
			return t;
		}
		// (429: RomM's limit on polls, as a slow_down)
		std::string d = parsed ? v["detail"].str() : "";
		if (status == 400 && d == "authorization_pending") t.state = DeviceToken::PENDING;
		else if ((status == 400 && d == "slow_down") || status == 429) t.state = DeviceToken::SLOW_DOWN;
		else if (status == 400 && d == "access_denied") t.state = DeviceToken::DENIED;
		else if (status == 400 && d == "expired_token") t.state = DeviceToken::EXPIRED;
		else t.error = unreadable(status);
		return t;
	}

	DeviceToken deviceToken(const Server& s, const std::string& deviceCode)
	{
		Request r;
		r.method = "POST";
		r.url = s.url + "/api/auth/device/token";
		r.type = "application/json";
		r.connectMs = s.connectMs;
		r.totalMs = s.totalMs;
		r.body = "{\"device_code\": " + jsonString(deviceCode) + "}";
		Response res = perform(r);
		return parseDeviceToken(res.body, res.status);
	}

	long getDevice(const Server& s, const std::string& id)
	{
		Request r;
		r.url = s.url + "/api/devices/" + id;
		r.token = s.token;
		r.connectMs = s.connectMs;
		r.totalMs = s.totalMs;
		return perform(r).status;
	}

	long deleteDevice(const Server& s, const std::string& id)
	{
		Request r;
		r.method = "DELETE";
		r.url = s.url + "/api/devices/" + id;
		r.token = s.token;
		r.connectMs = s.connectMs;
		r.totalMs = s.totalMs;
		return perform(r).status;
	}
}
