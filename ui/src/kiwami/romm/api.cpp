// The RomM helper (kiwami): RomM's requests and their replies (api.h).
#include "api.h"
#include "http.h"
#include "../json.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

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

		long long whole(const json::Value& v) { return v.type == json::Value::NUM ? (long long)v.n : 0; }

		// a request to the server, with its token and timeouts
		Request to(const Server& s, const char* method, const std::string& path)
		{
			Request r;
			r.method = method;
			r.url = s.url + path;
			r.token = s.token;
			r.connectMs = s.connectMs;
			r.totalMs = s.totalMs;
			return r;
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

	std::string urlEscape(const std::string& s)
	{
		static const char* HEX = "0123456789ABCDEF";
		std::string e;
		for (unsigned char c : s)
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') e += (char)c;
			else { e += '%'; e += HEX[c >> 4]; e += HEX[c & 15]; }
		return e;
	}

	std::string isoTime(long long ms)
	{
		time_t t = (time_t)(ms / 1000);
		struct tm g;
		gmtime_r(&t, &g);
		char b[96];
		snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min,
			g.tm_sec, (int)(ms % 1000));
		return b;
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

	// ---- the library ------------------------------------------------------------------------------------------------
	Platforms parsePlatforms(const std::string& body, long status)
	{
		Platforms p;
		p.status = status;
		json::Value v;
		if (status != 200 || !json::parse(body, v) || v.type != json::Value::ARR) return p;
		for (auto& e : v.arr)
		{
			Platform pl;
			pl.id = (long)whole(e["id"]);
			pl.slug = e["slug"].str();
			pl.name = e["name"].str();
			pl.romCount = (long)whole(e["rom_count"]);
			if (pl.id && !pl.slug.empty()) p.list.push_back(pl);
		}
		p.ok = true;
		return p;
	}

	Platforms platforms(const Server& s)
	{
		Response res = perform(to(s, "GET", "/api/platforms"));
		return parsePlatforms(res.body, res.status);
	}

	namespace
	{
		Rom romOf(const json::Value& e)
		{
			Rom rom;
			rom.id = (long)whole(e["id"]);
			rom.fsName = e["fs_name"].str();
			rom.name = e["name"].str();
			rom.size = whole(e["fs_size_bytes"]);
			rom.platformSlug = e["platform_slug"].str();
			rom.simple = e["has_simple_single_file"].type != json::Value::BOOL || e["has_simple_single_file"].b;
			// each file's path in the game's folder, as get_rom_content names it in its zip (file_name_for_download)
			std::string folder = e["full_path"].str() + "/";
			for (auto& f : e["files"].arr)
			{
				RomFile rf;
				std::string full = f["full_path"].str();
				rf.path = !rom.simple && full.compare(0, folder.size(), folder) == 0 ? full.substr(folder.size()) : f["file_name"].str();
				rf.size = whole(f["file_size_bytes"]);
				if (!rf.path.empty()) rom.files.push_back(rf);
			}
			return rom;
		}
	}

	Roms parseRoms(const std::string& body, long status)
	{
		Roms r;
		r.status = status;
		json::Value v;
		if (status != 200 || !json::parse(body, v) || v["items"].type != json::Value::ARR) return r;
		r.total = (long)whole(v["total"]);
		for (auto& e : v["items"].arr)
		{
			Rom rom = romOf(e);
			if (rom.id && !rom.fsName.empty()) r.items.push_back(rom);
		}
		r.ok = true;
		return r;
	}

	Roms roms(const Server& s, long platform, const std::string& search, long offset, long limit, const char* order)
	{
		std::string q = "/api/roms?platform_ids=" + std::to_string(platform) + "&offset=" + std::to_string(offset) + "&limit="
			+ std::to_string(limit) + "&order_by=" + order + "&order_dir=asc";
		if (!search.empty()) q += "&search_term=" + urlEscape(search);
		Response res = perform(to(s, "GET", q));
		return parseRoms(res.body, res.status);
	}

	RomDetail parseRomDetail(const std::string& body, long status)
	{
		RomDetail d;
		d.status = status;
		json::Value v;
		if (status != 200 || !json::parse(body, v) || v.type != json::Value::OBJ) return d;
		d.rom = romOf(v);
		d.ok = d.rom.id && !d.rom.fsName.empty();
		return d;
	}

	RomDetail romDetail(const Server& s, long id)
	{
		Response res = perform(to(s, "GET", "/api/roms/" + std::to_string(id)));
		return parseRomDetail(res.body, res.status);
	}

	Firmwares parseFirmware(const std::string& body, long status)
	{
		Firmwares f;
		f.status = status;
		json::Value v;
		if (status != 200 || !json::parse(body, v) || v.type != json::Value::ARR) return f;
		for (auto& e : v.arr)
		{
			Firmware fw;
			fw.id = (long)whole(e["id"]);
			fw.platformId = (long)whole(e["platform_id"]);
			fw.fileName = e["file_name"].str();
			fw.size = whole(e["file_size_bytes"]);
			fw.md5 = e["md5_hash"].str();
			if (fw.id && !fw.fileName.empty() && !e["missing_from_fs"].boolean()) f.list.push_back(fw);
		}
		f.ok = true;
		return f;
	}

	Firmwares firmware(const Server& s, long platform)
	{
		Response res = perform(to(s, "GET", "/api/firmware?platform_id=" + std::to_string(platform)));
		return parseFirmware(res.body, res.status);
	}

	namespace
	{
		Response content(const Server& s, const std::string& path, const std::string& file, long long resume, bool range,
			const std::function<void(long long, long long)>& progress)
		{
			Request r = to(s, "GET", path);
			r.range = range;
			r.totalMs = 0;
			r.lowSpeedBytes = 1024;
			r.lowSpeedSecs = 30;
			r.file = file;
			r.resume = resume;
			r.progress = progress;
			return perform(r);
		}
	}

	Response romContent(const Server& s, const Rom& r, const std::string& file, long long resume,
		const std::function<void(long long, long long)>& progress)
	{
		return content(s, "/api/roms/" + std::to_string(r.id) + "/content/" + urlEscape(r.fsName), file, resume, r.files.size() > 1, progress);
	}

	Response firmwareContent(const Server& s, const Firmware& f, const std::string& file, long long resume,
		const std::function<void(long long, long long)>& progress)
	{
		return content(s, "/api/firmware/" + std::to_string(f.id) + "/content/" + urlEscape(f.fileName), file, resume, false, progress);
	}

	// ---- the saves' sync --------------------------------------------------------------------------------------------
	Negotiation parseNegotiation(const std::string& body, long status)
	{
		Negotiation n;
		n.status = status;
		json::Value v;
		if (status != 200 || !json::parse(body, v) || v["operations"].type != json::Value::ARR)
		{
			std::string why = status ? detail(body) : "";
			n.error = why.empty() ? unreadable(status) : why + " (" + std::to_string(status) + ")";
			return n;
		}
		n.session = (long)whole(v["session_id"]);
		for (auto& e : v["operations"].arr)
		{
			Operation o;
			o.action = e["action"].str();
			o.romId = (long)whole(e["rom_id"]);
			o.saveId = (long)whole(e["save_id"]);
			o.slot = e["slot"].str();
			o.fileName = e["file_name"].str();
			o.emulator = e["emulator"].str();
			o.serverHash = e["server_content_hash"].str();
			o.serverTime = e["server_updated_at"].str();
			if (!o.action.empty() && o.romId) n.ops.push_back(o);
		}
		n.ok = true;
		return n;
	}

	Negotiation negotiate(const Server& s, const std::string& deviceId, const std::vector<ClientSave>& saves, const std::vector<long>& romIds)
	{
		Request r = to(s, "POST", "/api/sync/negotiate");
		r.type = "application/json";
		std::string list, ids;
		for (auto& c : saves)
			list += std::string(list.empty() ? "" : ", ") + "{\"rom_id\": " + std::to_string(c.romId) + ", \"file_name\": " + jsonString(c.fileName)
				+ ", \"slot\": \"autosave\", \"emulator\": " + jsonString(c.emulator) + ", \"content_hash\": " + jsonString(c.hash)
				+ ", \"updated_at\": " + jsonString(isoTime(c.mtimeMs)) + ", \"file_size_bytes\": " + std::to_string(c.size) + "}";
		for (long id : romIds) ids += (ids.empty() ? "" : ", ") + std::to_string(id);
		r.body = "{\"device_id\": " + jsonString(deviceId) + ", \"saves\": [" + list + "], \"rom_ids\": [" + ids + "]}";
		Response res = perform(r);
		return parseNegotiation(res.body, res.status);
	}

	Upload uploadSave(const Server& s, const ClientSave& save, const std::string& deviceId, long session, bool overwrite,
		const std::string& bytes)
	{
		std::string q = "/api/saves?rom_id=" + std::to_string(save.romId) + "&emulator=" + urlEscape(save.emulator)
			+ "&slot=autosave&device_id=" + urlEscape(deviceId) + "&content_hash=" + save.hash + "&overwrite="
			+ (overwrite ? "true" : "false") + "&autocleanup=true";
		if (session) q += "&session_id=" + std::to_string(session);
		Request r = to(s, "POST", q);
		r.body = multipart("saveFile", save.fileName, bytes, r.type);
		Response res = perform(r);
		Upload u;
		u.status = res.status;
		u.conflict = res.status == 409;
		json::Value v;
		if (res.status == 200 && json::parse(res.body, v)) { u.saveId = (long)whole(v["id"]); u.ok = true; }
		return u;
	}

	long downloadSave(const Server& s, long id, const std::string& deviceId, long session, std::string& bytes)
	{
		std::string q = "/api/saves/" + std::to_string(id) + "/content?device_id=" + urlEscape(deviceId);
		if (session) q += "&session_id=" + std::to_string(session);
		Response res = perform(to(s, "GET", q));
		bytes = res.status == 200 ? res.body : "";
		return res.status;
	}

	long confirmDownload(const Server& s, long id, const std::string& deviceId, const std::string& hash)
	{
		Request r = to(s, "POST", "/api/saves/" + std::to_string(id) + "/downloaded");
		r.type = "application/json";
		r.body = "{\"device_id\": " + jsonString(deviceId) + ", \"content_hash\": " + (hash.empty() ? "null" : jsonString(hash)) + "}";
		return perform(r).status;
	}

	namespace
	{
		// the plays as RomM's PlaySessionEntry list, "" when none is long enough to send
		std::string playList(const std::vector<PlaySession>& plays)
		{
			std::string list;
			for (auto& p : plays)
			{
				if (!p.romId || p.end < p.start + 1) continue;
				list += std::string(list.empty() ? "" : ", ") + "{\"rom_id\": " + std::to_string(p.romId) + ", \"save_slot\": \"autosave\", "
					+ "\"start_time\": " + jsonString(isoTime(p.start * 1000)) + ", \"end_time\": " + jsonString(isoTime(p.end * 1000))
					+ ", \"duration_ms\": " + std::to_string((p.end - p.start) * 1000) + "}";
			}
			return list;
		}
	}

	long playSessions(const Server& s, const std::string& deviceId, const std::vector<PlaySession>& plays)
	{
		std::string list = playList(plays);
		if (list.empty()) return 201;
		Request r = to(s, "POST", "/api/play-sessions");
		r.type = "application/json";
		r.body = "{\"device_id\": " + jsonString(deviceId) + ", \"sessions\": [" + list + "]}";
		return perform(r).status;
	}

	long complete(const Server& s, long session, int done, int failed, const std::vector<PlaySession>& plays)
	{
		Request r = to(s, "POST", "/api/sync/sessions/" + std::to_string(session) + "/complete");
		r.type = "application/json";
		std::string list = playList(plays);
		r.body = "{\"operations_completed\": " + std::to_string(done) + ", \"operations_failed\": " + std::to_string(failed)
			+ (list.empty() ? "" : ", \"play_sessions\": [" + list + "]") + "}";
		return perform(r).status;
	}
}
