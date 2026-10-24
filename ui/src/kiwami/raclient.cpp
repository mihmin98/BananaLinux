// banana-ui (kiwami): RetroAchievements' server (raclient.h). The transport is libcurl, dlopen()ed as upstream's
// (settings.cpp's Curl), in a copy of its own; the replies' fields are rcheevos 12's (rc_api_user.c, rc_api_runtime.c).
#include "raclient.h"
#include "json.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/stat.h>

namespace banana::kiwami::ra
{
	const Console CONSOLES[9] = {
		{ "nds", 18, "Nintendo DS" }, { "gba", 5, "Game Boy Advance" }, { "gbc", 6, "Game Boy Color" }, { "gb", 4, "Game Boy" },
		{ "nes", 7, "NES" }, { "snes", 3, "Super Nintendo" }, { "megadrive", 1, "Mega Drive" }, { "psx", 12, "PlayStation" },
		{ "n64", 2, "Nintendo 64" },
	};

	namespace
	{
		struct Curl
		{
			void* (*init)(void) = nullptr;
			int (*setopt)(void*, int, ...) = nullptr;
			int (*perform)(void*) = nullptr;
			int (*getinfo)(void*, int, ...) = nullptr;
			void (*cleanup)(void*) = nullptr;
			bool ok = false;
			Curl()
			{
				void* l = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
				if (!l) return;
				init = (void* (*)(void))dlsym(l, "curl_easy_init");
				setopt = (int (*)(void*, int, ...))dlsym(l, "curl_easy_setopt");
				perform = (int (*)(void*))dlsym(l, "curl_easy_perform");
				getinfo = (int (*)(void*, int, ...))dlsym(l, "curl_easy_getinfo");
				cleanup = (void (*)(void*))dlsym(l, "curl_easy_cleanup");
				ok = init && setopt && perform && getinfo && cleanup;
			}
		};
		const Curl& curl() { static const Curl c; return c; }	// (loaded once, by the first request's thread)

		size_t onBody(char* p, size_t sz, size_t n, void* u) { ((std::string*)u)->append(p, sz * n); return sz * n; }

		std::string base(const char* env, const char* dflt)
		{
			const char* e = getenv(env);
			return e && *e ? e : dflt;
		}

		// a GET or (with fields) a POST: the HTTP status, 0 when there was no answer; the time it took in ms
		long request(const std::string& url, const std::string* fields, std::string& body, double& ms)
		{
			auto t0 = std::chrono::steady_clock::now();
			long status = 0;
			const Curl& cu = curl();
			if (void* h = cu.ok ? cu.init() : nullptr)
			{
				enum { WRITEDATA = 10001, URL = 10002, POSTFIELDS = 10015, USERAGENT = 10018, WRITEFUNCTION = 20011, TIMEOUT = 13,
					NOSIGNAL = 99, RESPONSE_CODE = 0x200002 };
				cu.setopt(h, URL, url.c_str());
				if (fields) cu.setopt(h, POSTFIELDS, fields->c_str());
				cu.setopt(h, USERAGENT, "BananaOS/0.2 rcheevos");
				cu.setopt(h, NOSIGNAL, 1L);
				cu.setopt(h, TIMEOUT, 20L);
				cu.setopt(h, WRITEFUNCTION, onBody);
				cu.setopt(h, WRITEDATA, &body);
				if (cu.perform(h) == 0) cu.getinfo(h, RESPONSE_CODE, &status);
				else body.clear();
				cu.cleanup(h);
			}
			ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			return status;
		}

		// dorequest.php's r with its fields (name, value), escaped; the one log line
		long post(const char* r, const std::vector<std::pair<const char*, std::string>>& fields, std::string& body)
		{
			std::string form = std::string("r=") + r;
			for (auto& f : fields) form += std::string("&") + f.first + "=" + escape(f.second);
			double ms = 0;
			long status = request(base("KIWAMI_RA_URL", "https://retroachievements.org") + "/dorequest.php", &form, body, ms);
			if (status) fprintf(stderr, "banana-ui: ra: %s %ld %.0f ms\n", r, status, ms);
			else fprintf(stderr, "banana-ui: ra: %s no connection %.0f ms\n", r, ms);
			return status;
		}

		// a file: to <path>.part, then renamed; a 200 of a few bytes at least, or nothing is kept
		bool fetch(const char* what, const std::string& url, const std::string& path)
		{
			std::string body;
			double ms = 0;
			long status = request(url, nullptr, body, ms);
			if (status) fprintf(stderr, "banana-ui: ra: %s %ld %.0f ms\n", what, status, ms);
			else fprintf(stderr, "banana-ui: ra: %s no connection %.0f ms\n", what, ms);
			if (status != 200 || body.size() < 8) return false;
			std::string tmp = path + ".part";
			FILE* f = fopen(tmp.c_str(), "wb");
			if (!f) return false;
			bool ok = fwrite(body.data(), 1, body.size(), f) == body.size();
			ok = fclose(f) == 0 && ok;
			if (ok && rename(tmp.c_str(), path.c_str()) == 0) return true;
			remove(tmp.c_str());
			return false;
		}

		bool makeDir(const std::string& dir)
		{
			for (size_t i = 1; i <= dir.size(); i++)
				if (i == dir.size() || dir[i] == '/')
					if (mkdir(dir.substr(0, i).c_str(), 0755) != 0 && errno != EEXIST) return false;
			return true;
		}

		// a number as the server writes it: a JSON number, or a string of one
		double num(const json::Value& v)
		{
			return v.type == json::Value::NUM ? v.n : v.type == json::Value::STR ? strtod(v.s.c_str(), nullptr) : 0;
		}

		// the reply's JSON into v, and whether the server said yes; r's ok, refused and error set from it
		bool read(const std::string& body, long status, json::Value& v, Reply& r)
		{
			bool parsed = status && json::parse(body, v) && v.type == json::Value::OBJ;
			const json::Value& s = v["Success"];
			if (parsed && s.type == json::Value::BOOL && s.b && status == 200) { r.ok = true; return true; }
			if ((parsed && s.type == json::Value::BOOL && !s.b) || status == 401 || status == 403)
			{
				r.refused = true;
				r.error = parsed && !v["Error"].str().empty() ? v["Error"].str() : "refused (" + std::to_string(status) + ")";
				return false;
			}
			r.error = !status ? "no connection" : "unreadable answer (" + std::to_string(status) + ")";
			return false;
		}
	}

	std::string escape(const std::string& s)
	{
		static const char* HEX = "0123456789ABCDEF";
		std::string e;
		for (unsigned char c : s)
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') e += (char)c;
			else { e += '%'; e += HEX[c >> 4]; e += HEX[c & 15]; }
		return e;
	}

	static bool plain(const std::string& s)
	{
		if (s.empty() || s.size() > 64) return false;
		for (char c : s)
			if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
		return true;
	}

	std::string fileName(const std::string& name)
	{
		if (!plain(name)) return "";
		std::string l = name;
		for (auto& c : l) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		return l;
	}

	Login parseLogin(const std::string& body, long status)
	{
		Login l;
		json::Value v;
		if (!read(body, status, v, l)) return l;
		l.user = v["User"].str();
		l.token = v["Token"].str();
		if (l.user.empty() || l.token.empty()) { l.ok = false; l.error = "unreadable answer (no user or token)"; return l; }
		l.score = (long)num(v["Score"]);
		l.softScore = (long)num(v["SoftcoreScore"]);
		l.messages = (long)num(v["Messages"]);
		l.avatarUrl = v["AvatarUrl"].str();
		return l;
	}

	Progress parseProgress(const std::string& body, long status)
	{
		Progress p;
		json::Value v;
		if (!read(body, status, v, p)) return p;
		// an object of the games played, by ID ([] when there are none: PHP's empty array)
		const json::Value& games = v["Response"];
		if (games.type != json::Value::OBJ && games.type != json::Value::ARR)
		{
			p.ok = false;
			p.error = "unreadable answer (no Response)";
			return p;
		}
		for (auto& g : games.obj)
		{
			GameProgress gp;
			gp.id = (unsigned)strtoul(g.first.c_str(), nullptr, 10);
			gp.total = (int)num(g.second["Achievements"]);
			gp.unlocked = (int)num(g.second["Unlocked"]);
			gp.unlockedHardcore = (int)num(g.second["UnlockedHardcore"]);
			p.games.push_back(gp);
			int any = std::max(gp.unlocked, gp.unlockedHardcore);
			p.started++;
			p.unlocked += any;
			if (gp.total > 0 && gp.unlockedHardcore >= gp.total) p.mastered++;
			else if (gp.total > 0 && any >= gp.total) p.completed++;
		}
		return p;
	}

	GameId parseGameId(const std::string& body, long status)
	{
		GameId g;
		json::Value v;
		if (!read(body, status, v, g)) return g;
		g.id = (unsigned)num(v["GameID"]);
		return g;
	}

	Patch parsePatch(const std::string& body, long status)
	{
		Patch p;
		json::Value v;
		if (!read(body, status, v, p)) return p;
		const json::Value& d = v["PatchData"];
		if (d.type != json::Value::OBJ) { p.ok = false; p.error = "unreadable answer (no PatchData)"; return p; }
		p.title = d["Title"].str();
		for (auto& a : d["Achievements"].arr)
		{
			if ((int)num(a["Flags"]) != 3) continue;	// core achievements only (5: unofficial)
			Achievement x;
			x.id = (unsigned)num(a["ID"]);
			x.title = a["Title"].str();
			x.description = a["Description"].str();
			x.badge = a["BadgeName"].str();
			x.points = (int)num(a["Points"]);
			p.achievements.push_back(x);
		}
		return p;
	}

	Unlocks parseUnlocks(const std::string& body, long status)
	{
		Unlocks u;
		json::Value v;
		if (!read(body, status, v, u)) return u;
		for (auto& id : v["UserUnlocks"].arr) u.ids.push_back((unsigned)num(id));
		return u;
	}

	Login login2(const std::string& user, const std::string& password, const std::string& token)
	{
		std::string body;
		long status = post("login2", { { "u", user }, password.empty() ? std::make_pair("t", token) : std::make_pair("p", password) }, body);
		return parseLogin(body, status);
	}

	Progress allProgress(const std::string& user, const std::string& token, unsigned console)
	{
		std::string body;
		long status = post("allprogress", { { "u", user }, { "t", token }, { "c", std::to_string(console) } }, body);
		return parseProgress(body, status);
	}

	GameId gameId(const std::string& hash)
	{
		std::string body;
		long status = post("gameid", { { "m", hash } }, body);
		return parseGameId(body, status);
	}

	Patch patch(const std::string& user, const std::string& token, unsigned game)
	{
		std::string body;
		long status = post("patch", { { "u", user }, { "t", token }, { "g", std::to_string(game) } }, body);
		return parsePatch(body, status);
	}

	Unlocks unlocks(const std::string& user, const std::string& token, unsigned game, bool hardcore)
	{
		std::string body;
		long status = post("unlocks", { { "u", user }, { "t", token }, { "g", std::to_string(game) }, { "h", hardcore ? "1" : "0" } }, body);
		return parseUnlocks(body, status);
	}

	std::string avatar(const std::string& user, const std::string& avatarUrl, const std::string& dir)
	{
		std::string name = fileName(user);
		if (name.empty() || !makeDir(dir)) return "";
		std::string url = avatarUrl.compare(0, 8, "https://") == 0 || avatarUrl.compare(0, 7, "http://") == 0 ? avatarUrl
			: base("KIWAMI_RA_MEDIA", "https://media.retroachievements.org") + "/UserPic/" + user + ".png";
		std::string path = dir + "/" + name + ".png";
		return fetch("avatar", url, path) ? path : "";
	}

	std::string badge(const std::string& name, const std::string& dir)
	{
		if (!plain(name) || !makeDir(dir)) return "";
		std::string path = dir + "/" + name + ".png";
		return fetch("badge", base("KIWAMI_RA_MEDIA", "https://media.retroachievements.org") + "/Badge/" + name + ".png", path) ? path : "";
	}
}
