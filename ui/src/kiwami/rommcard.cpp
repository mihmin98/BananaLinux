// banana-ui (kiwami): RomM on the card (rommcard.h).
#include "rommcard.h"
#include "conf.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace banana::kiwami::romm
{
	namespace
	{
		std::string env(const char* name, const char* dflt)
		{
			const char* e = getenv(name);
			std::string s = e && *e ? e : dflt;
			while (s.size() > 1 && s.back() == '/') s.pop_back();
			return s;
		}

		std::vector<std::string> fields(const std::string& ln)
		{
			std::vector<std::string> f;
			std::istringstream in(ln);
			std::string x;
			while (std::getline(in, x, '\t')) f.push_back(x);
			return f;
		}

		std::vector<std::string> lines(const std::string& path)
		{
			std::string text, ln;
			bool there = false;
			readFile(path, text, there);
			std::vector<std::string> l;
			std::istringstream in(text);
			while (std::getline(in, ln)) if (!ln.empty()) l.push_back(ln);
			return l;
		}

		bool writeLines(const Card& c, const std::string& path, const std::vector<std::string>& l, std::string& error)
		{
			std::string text;
			for (auto& ln : l) text += ln + "\n";
			if (!makeDir(c.dir)) { error = "can't make " + c.dir; return false; }
			return replaceFile(path, text, error);
		}

		std::vector<std::string> pendingLines(const Pending& p)
		{
			std::vector<std::string> l;
			for (auto& x : p.pushes) l.push_back("push\t" + x.system + "\t" + x.rom);
			for (auto& x : p.plays)
				l.push_back("play\t" + x.system + "\t" + x.rom + "\t" + std::to_string(x.start) + "\t" + std::to_string(x.end));
			return l;
		}

		// the pending file's lock, held while the object lives
		struct PendingLock
		{
			int fd;
			explicit PendingLock(const Card& c)
			{
				makeDir(c.run);
				fd = open((c.run + "/romm-pending.lock").c_str(), O_RDWR | O_CREAT, 0644);
				if (fd >= 0) flock(fd, LOCK_EX);
			}
			~PendingLock() { if (fd >= 0) close(fd); }
		};
	}

	Card::Card()
	{
		card = env("KIWAMI_ROMM_CARD", "/data");
		data = card + "/banana";
		accounts = data + "/accounts.conf";
		settings = data + "/settings.conf";
		dir = data + "/romm";
		logs = data + "/logs";
		run = env("KIWAMI_RUN", "/run/banana");
		status = run + "/romm.status";
		etc = env("KIWAMI_ROMM_ETC", "/etc/banana");
		libretro = env("KIWAMI_LIBRETRO", "/usr/lib/libretro");
		ndsSaves = env("KIWAMI_ROMM_NDS_SAVES", "/usr/libexec/banana/nds-saves");
	}

	Account account(const Card& c)
	{
		Account a;
		a.url = confGet(c.accounts, "romm_url");
		a.token = confGet(c.accounts, "romm_token");
		a.deviceId = confGet(c.accounts, "romm_device_id");
		return a;
	}

	std::string normalServer(const std::string& url)
	{
		size_t b = url.find_first_not_of(" \t\r\n"), e = url.find_last_not_of(" \t\r\n");
		if (b == std::string::npos) return "";
		std::string s = url.substr(b, e - b + 1);
		if (s.find("://") == std::string::npos) s = "http://" + s;
		while (!s.empty() && s.back() == '/' && s.size() > s.find("://") + 3) s.pop_back();
		return s;
	}

	bool setServer(const Card& c, const std::string& url, std::string& error)
	{
		std::string s = normalServer(url);
		return confEdit(c.accounts, { { "romm_url", s, s.empty() } }, "=", true, error);
	}

	bool paired(const Card& c, const std::string& token, const std::string& deviceId, std::string& error)
	{
		return confEdit(c.accounts, { { "romm_token", token }, { "romm_device_id", deviceId } }, "=", true, error);
	}

	bool forget(const Card& c, std::string& error)
	{
		return confEdit(c.accounts, { { "romm_token", "", true }, { "romm_device_id", "", true } }, "=", false, error);
	}

	std::string host(const std::string& url)
	{
		size_t s = url.find("://");
		std::string h = s == std::string::npos ? url : url.substr(s + 3);
		size_t e = h.find_first_of("/?#");
		if (e != std::string::npos) h = h.substr(0, e);
		size_t at = h.rfind('@');		// (a user:password@ the address carried)
		return at == std::string::npos ? h : h.substr(at + 1);
	}

	std::string setting(const Card& c, const std::string& key, const std::string& dflt)
	{
		std::string v = confGet(c.settings, key);
		if (v.empty()) v = confGet(c.etc + "/settings.conf", key);
		return v.empty() ? dflt : v;
	}

	bool makeDir(const std::string& dir)
	{
		for (size_t i = 1; i <= dir.size(); i++)
			if (i == dir.size() || dir[i] == '/')
				if (mkdir(dir.substr(0, i).c_str(), 0755) != 0 && errno != EEXIST) return false;
		return true;
	}

	std::string deviceIdentifier(const Card& c)
	{
		std::string path = c.dir + "/identifier", id, s;
		bool there = false;
		if (readFile(path, s, there))
		{
			for (char ch : s)
				if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')) id += ch;
			if (id.size() == 16) return id;
		}
		unsigned char r[8] = {};
		FILE* f = fopen("/dev/urandom", "rb");
		bool ok = f && fread(r, 1, sizeof r, f) == sizeof r;
		if (f) fclose(f);
		if (!ok) return "";
		static const char* HEX = "0123456789abcdef";
		id.clear();
		for (unsigned char b : r) { id += HEX[b >> 4]; id += HEX[b & 15]; }
		std::string error;
		if (!makeDir(c.dir) || !replaceFile(path, id + "\n", error)) return "";
		return id;
	}

	Pending pending(const Card& c)
	{
		Pending p;
		for (auto& ln : lines(c.dir + "/pending"))
		{
			std::vector<std::string> f = fields(ln);
			if (f.size() == 3 && f[0] == "push") p.pushes.push_back({ f[1], f[2] });
			else if (f.size() == 5 && f[0] == "play") p.plays.push_back({ f[1], f[2], atoll(f[3].c_str()), atoll(f[4].c_str()) });
		}
		return p;
	}

	bool addPending(const Card& c, const Pending& add, std::string& error)
	{
		PendingLock lock(c);
		std::vector<std::string> l = lines(c.dir + "/pending"), n = pendingLines(add);
		size_t before = l.size();
		for (auto& x : n)
		{
			bool there = false;
			for (auto& y : l) there |= x == y;
			if (!there) l.push_back(x);
		}
		return l.size() == before || writeLines(c, c.dir + "/pending", l, error);
	}

	bool removePending(const Card& c, const Pending& done, std::string& error)
	{
		PendingLock lock(c);
		std::vector<std::string> l = lines(c.dir + "/pending"), d = pendingLines(done), out;
		for (auto& x : l)
		{
			bool gone = false;
			for (auto& y : d) gone |= x == y;
			if (!gone) out.push_back(x);
		}
		return out.size() == l.size() || writeLines(c, c.dir + "/pending", out, error);
	}

	std::vector<Conflict> conflicts(const Card& c)
	{
		std::vector<Conflict> k;
		for (auto& ln : lines(c.dir + "/conflicts"))
		{
			std::vector<std::string> f = fields(ln);
			if (f.size() < 6) continue;
			k.push_back({ f[0], f[1], atol(f[2].c_str()), atol(f[3].c_str()), f[4], atoll(f[5].c_str()) });
		}
		return k;
	}

	bool noteConflict(const Card& c, const Conflict& k, std::string& error)
	{
		std::vector<std::string> out;
		std::string key = k.system + "\t" + k.rom + "\t";
		for (auto& ln : lines(c.dir + "/conflicts")) if (ln.compare(0, key.size(), key) != 0) out.push_back(ln);
		out.push_back(key + std::to_string(k.romId) + "\t" + std::to_string(k.saveId) + "\t" + k.serverTime + "\t" + std::to_string(k.noted));
		return writeLines(c, c.dir + "/conflicts", out, error);
	}

	bool clearConflict(const Card& c, const std::string& system, const std::string& rom, std::string& error)
	{
		std::vector<std::string> l = lines(c.dir + "/conflicts"), out;
		std::string key = system + "\t" + rom + "\t";
		for (auto& ln : l) if (ln.compare(0, key.size(), key) != 0) out.push_back(ln);
		return out.size() == l.size() || writeLines(c, c.dir + "/conflicts", out, error);
	}

	Last last(const Card& c)
	{
		Last l;
		std::vector<std::string> ls = lines(c.dir + "/last");
		if (ls.empty()) return l;
		std::vector<std::string> f = fields(ls[0]);
		if (f.size() < 6) return l;
		l.when = atoll(f[0].c_str());
		l.state = f[1];
		l.sent = atoi(f[2].c_str());
		l.fetched = atoi(f[3].c_str());
		l.conflicts = atoi(f[4].c_str());
		l.skipped = atoi(f[5].c_str());
		return l;
	}

	bool setLast(const Card& c, const Last& l, std::string& error)
	{
		return writeLines(c, c.dir + "/last", { std::to_string(l.when) + "\t" + l.state + "\t" + std::to_string(l.sent) + "\t"
			+ std::to_string(l.fetched) + "\t" + std::to_string(l.conflicts) + "\t" + std::to_string(l.skipped) }, error);
	}

	std::string syncedHash(const Card& c, long romId)
	{
		std::string key = std::to_string(romId) + "\t";
		for (auto& ln : lines(c.dir + "/synced")) if (ln.compare(0, key.size(), key) == 0) return ln.substr(key.size());
		return "";
	}

	bool setSynced(const Card& c, long romId, const std::string& hash, std::string& error)
	{
		std::string key = std::to_string(romId) + "\t";
		std::vector<std::string> l = lines(c.dir + "/synced"), out;
		for (auto& ln : l) if (ln.compare(0, key.size(), key) != 0) out.push_back(ln);
		if (!hash.empty()) out.push_back(key + hash);
		if (out == l) return true;
		return writeLines(c, c.dir + "/synced", out, error);
	}
}
