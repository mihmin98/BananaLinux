// banana-ui (kiwami): RomM on the card (rommcard.h).
#include "rommcard.h"
#include "conf.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

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
}
