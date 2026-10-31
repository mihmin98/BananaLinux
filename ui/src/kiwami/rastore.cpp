// banana-ui (kiwami): the RetroAchievements account on the card (rastore.h).
#include "rastore.h"
#include "raclient.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace banana::kiwami::ra
{
	namespace
	{
		// a line's value when its key is key (spaces round both), else false
		bool keyed(const std::string& ln, const std::string& key, std::string* value = nullptr)
		{
			size_t i = ln.find_first_not_of(" \t");
			if (i == std::string::npos || ln.compare(i, key.size(), key) != 0) return false;
			i = ln.find_first_not_of(" \t", i + key.size());
			if (i == std::string::npos || ln[i] != '=') return false;
			if (value)
			{
				size_t b = ln.find_first_not_of(" \t", i + 1), e = ln.find_last_not_of(" \t\r");
				*value = b == std::string::npos || e < b ? "" : ln.substr(b, e - b + 1);
				if (value->size() >= 2 && (*value)[0] == '"' && value->back() == '"') *value = value->substr(1, value->size() - 2);
			}
			return true;
		}

		// the whole file into s; false when it is there but can't be read (a card error), or (with s "") isn't there
		bool slurp(const std::string& path, std::string& s, bool& there)
		{
			s.clear();
			there = access(path.c_str(), F_OK) == 0;
			std::ifstream f(path, std::ios::binary);
			if (!f) return false;
			std::stringstream ss;
			ss << f.rdbuf();
			s = ss.str();
			return true;
		}

		// the file replaced by text: through <path>.new and a rename, so a full card or an error keeps the old one
		bool replace(const std::string& path, const std::string& text, std::string& error)
		{
			std::string tmp = path + ".new";
			int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
			bool ok = fd >= 0;
			ok = ok && write(fd, text.data(), text.size()) == (ssize_t)text.size();
			ok = ok && fsync(fd) == 0;
			int err = errno;
			if (fd >= 0 && close(fd) != 0 && ok) { ok = false; err = errno; }
			if (ok && rename(tmp.c_str(), path.c_str()) != 0) { ok = false; err = errno; }
			if (ok) return true;
			if (fd >= 0) unlink(tmp.c_str());
			error = "can't write " + path + ": " + strerror(err);
			fprintf(stderr, "banana-ui: ra: %s\n", error.c_str());
			return false;
		}

		// key=value files: each key set (its first line replaced, the others dropped, or a line added at the end) or,
		// with del, every line of it dropped; every other line kept in its order. A file that isn't there is made only
		// when create; one that doesn't change isn't written
		struct Edit { std::string key, value; bool del = false; };
		bool edit(const std::string& path, const std::vector<Edit>& edits, const char* sep, bool create, std::string& error)
		{
			std::string old;
			bool there = false;
			if (!slurp(path, old, there) && there)
			{
				error = "can't read " + path;
				fprintf(stderr, "banana-ui: ra: %s\n", error.c_str());
				return false;
			}
			if (!there && !create) return true;
			std::vector<bool> done(edits.size(), false);
			std::string out, ln;
			std::istringstream in(old);
			while (std::getline(in, ln))
			{
				size_t e = 0;
				while (e < edits.size() && !keyed(ln, edits[e].key)) e++;
				if (e == edits.size()) { out += ln + "\n"; continue; }
				if (edits[e].del || done[e]) continue;
				out += edits[e].key + sep + edits[e].value + "\n";
				done[e] = true;
			}
			for (size_t e = 0; e < edits.size(); e++)
				if (!edits[e].del && !done[e]) out += edits[e].key + sep + edits[e].value + "\n";
			if (there && out == old) return true;
			return replace(path, out, error);
		}

		bool makeDir(const std::string& dir)
		{
			for (size_t i = 1; i <= dir.size(); i++)
				if (i == dir.size() || dir[i] == '/')
					if (mkdir(dir.substr(0, i).c_str(), 0755) != 0 && errno != EEXIST) return false;
			return true;
		}

		// a name to the front of the remembered list (once)
		void remember(Accounts& a, const std::string& name)
		{
			if (name.empty()) return;
			for (size_t i = 0; i < a.remembered.size();)
				if (sameName(a.remembered[i], name)) a.remembered.erase(a.remembered.begin() + i);
				else i++;
			a.remembered.insert(a.remembered.begin(), name);
		}

		void forget(Accounts& a, const std::string& name)
		{
			for (size_t i = 0; i < a.remembered.size();)
				if (sameName(a.remembered[i], name)) a.remembered.erase(a.remembered.begin() + i);
				else i++;
		}

		bool writeList(const Card& c, const Accounts& a, std::string& error)
		{
			std::string text;
			if (!a.current.empty()) text += "current\t" + a.current + "\n";
			for (auto& n : a.remembered) text += "remembered\t" + n + "\n";
			std::string old;
			bool there = false;
			slurp(c.list, old, there);
			if (there && old == text) return true;
			return replace(c.list, text, error);
		}

		void logRemembered(const Accounts& a)
		{
			std::string l;
			for (auto& n : a.remembered) l += " " + n;
			fprintf(stderr, "banana-ui: ra: remembered:%s\n", l.empty() ? " none" : l.c_str());
		}
	}

	Card::Card(const std::string& d) : data(d)
	{
		while (data.size() > 1 && data.back() == '/') data.pop_back();
		size_t s = data.rfind('/');
		std::string up = s == std::string::npos ? "." : data.substr(0, s);
		accounts = data + "/accounts.conf";
		list = data + "/ra-accounts";
		avatars = data + "/cache/ra-avatars";
		stats = data + "/cache/ra-stats";
		hashes = data + "/cache/ra-hashes";
		badges = data + "/cache/ra-badges";
		retroarch = up + "/retroarch/retroarch.cfg";
		superdrastic = up + "/superdrastic/ra.token";
		bananads = up + "/bananads/ra.token";
	}

	bool sameName(const std::string& a, const std::string& b)
	{
		return a.size() == b.size() && strncasecmp(a.c_str(), b.c_str(), a.size()) == 0;
	}

	std::string confGet(const std::string& path, const std::string& key)
	{
		std::ifstream f(path);
		std::string ln, v, r;
		while (std::getline(f, ln))
			if (keyed(ln, key, &v)) r = v;
		return r;
	}

	Account account(const Card& c)
	{
		Account a;
		a.user = confGet(c.accounts, "ra_user");
		a.password = confGet(c.accounts, "ra_password");
		a.token = confGet(c.accounts, "ra_retroarch_token");
		return a;
	}

	Accounts accounts(const Card& c)
	{
		Accounts a;
		std::ifstream f(c.list);
		std::string ln;
		while (std::getline(f, ln))
		{
			if (!ln.empty() && ln.back() == '\r') ln.pop_back();
			size_t t = ln.find('\t');
			if (t == std::string::npos || t + 1 == ln.size()) continue;
			std::string kind = ln.substr(0, t), name = ln.substr(t + 1);
			if (kind == "current" && a.current.empty()) a.current = name;
			else if (kind == "remembered")
			{
				bool dup = sameName(name, a.current);
				for (auto& n : a.remembered) dup |= sameName(n, name);
				if (!dup) a.remembered.push_back(name);
			}
		}
		forget(a, a.current);
		return a;
	}

	bool signedIn(const Card& c, const std::string& name, const std::string& password, const std::string& token, std::string& error)
	{
		Accounts a = accounts(c);
		std::string prev = a.current.empty() ? account(c).user : a.current;
		if (!edit(c.accounts, { { "ra_user", name }, { "ra_password", password }, { "ra_retroarch_token", token, token.empty() } }, "=", true, error))
			return false;
		bool other = !prev.empty() && !sameName(prev, name);
		if (other) remember(a, prev);
		forget(a, name);
		a.current = name;
		fprintf(stderr, "banana-ui: ra: account %s on the card (%s)\n", name.c_str(), token.empty() ? "no token" : "token kept");
		if (!writeList(c, a, error)) return false;
		if (other) logRemembered(a);
		return true;
	}

	bool signOut(const Card& c, std::string& error)
	{
		const char* e = getenv("KIWAMI_RA_SCRUB");
		std::string scrub = e && *e ? e : "/usr/libexec/banana/ra-secrets scrub";
		int st = system(scrub.c_str());
		if (st != 0) fprintf(stderr, "banana-ui: ra: the scrub failed (%d), signing out all the same\n", WIFEXITED(st) ? WEXITSTATUS(st) : st);

		Accounts a = accounts(c);
		std::string user = account(c).user;	// (the scrub's, when RetroArch's own login replaced the account)
		// the secrets, each place tried whatever the others did; the first error is the one said
		std::string err;
		const std::vector<Edit> keys = { { "ra_user", "", true }, { "ra_password", "", true }, { "ra_retroarch_token", "", true } };
		const std::vector<Edit> off = { { "cheevos_enable", "\"false\"" }, { "cheevos_username", "\"\"" } };
		bool conf = edit(c.accounts, keys, "=", false, err);
		if (!conf) error = err;
		if (!edit(c.retroarch, off, " = ", false, err) && error.empty()) error = err;
		for (const std::string& t : { c.superdrastic, c.bananads })
			if (unlink(t.c_str()) != 0 && errno != ENOENT && error.empty())
			{
				error = "can't delete " + t + ": " + strerror(errno);
				fprintf(stderr, "banana-ui: ra: %s\n", error.c_str());
			}
		// the password still on the card: still signed in as far as the list goes
		if (!conf) return false;

		std::string name = !a.current.empty() ? a.current : user;
		if (!sameName(user, name)) remember(a, user);
		remember(a, name);
		a.current.clear();
		fprintf(stderr, "banana-ui: ra: signed out %s\n", name.empty() ? "(no account)" : name.c_str());
		if (!writeList(c, a, err)) { if (error.empty()) error = err; return false; }
		logRemembered(a);
		return error.empty();
	}

	bool remove(const Card& c, const std::string& name, std::string& error)
	{
		Accounts a = accounts(c);
		if (sameName(a.current, name)) { error = name + " is signed in"; return false; }
		forget(a, name);
		if (!writeList(c, a, error)) return false;
		std::string f = fileName(name);
		if (!f.empty())
		{
			unlink((c.avatars + "/" + f + ".png").c_str());
			unlink((c.stats + "/" + f).c_str());
		}
		fprintf(stderr, "banana-ui: ra: removed %s\n", name.c_str());
		logRemembered(a);
		return true;
	}

	bool adopt(const Card& c)
	{
		std::string user = account(c).user;
		Accounts a = accounts(c);
		if (sameName(user, a.current)) return false;
		std::string old = a.current;
		remember(a, old);
		forget(a, user);
		a.current = user;
		std::string error;
		if (!writeList(c, a, error)) return false;
		if (user.empty()) fprintf(stderr, "banana-ui: ra: %s no longer on the card, remembered\n", old.c_str());
		else if (old.empty()) fprintf(stderr, "banana-ui: ra: adopted %s\n", user.c_str());
		else fprintf(stderr, "banana-ui: ra: adopted %s, %s remembered\n", user.c_str(), old.c_str());
		return true;
	}

	bool needsToken(const Card& c)
	{
		Account a = account(c);
		return !a.user.empty() && !a.password.empty() && a.token.empty();
	}

	bool saveStats(const Card& c, const std::string& name, const Stats& s, std::string& error)
	{
		std::string f = fileName(name);
		if (f.empty()) { error = "not a name for a file: " + name; return false; }
		if (!makeDir(c.stats)) { error = "can't make " + c.stats + ": " + strerror(errno); return false; }
		std::string text = "when " + std::to_string(s.when) + "\nscore " + std::to_string(s.score) + "\nsoftscore "
			+ std::to_string(s.softScore) + "\nmessages " + std::to_string(s.messages) + "\n";
		for (int i = 0; i < 9; i++)
		{
			const SystemStats& y = s.systems[i];
			text += std::string(CONSOLES[i].system) + " " + std::to_string(y.started) + " " + std::to_string(y.completed) + " "
				+ std::to_string(y.mastered) + " " + std::to_string(y.unlocked) + "\n";
		}
		return replace(c.stats + "/" + f, text, error);
	}

	Stats loadStats(const Card& c, const std::string& name)
	{
		Stats s;
		std::string f = fileName(name);
		if (f.empty()) return s;
		std::ifstream in(c.stats + "/" + f);
		std::string ln;
		while (std::getline(in, ln))
		{
			std::istringstream w(ln);
			std::string k;
			w >> k;
			if (k == "when") w >> s.when;
			else if (k == "score") w >> s.score;
			else if (k == "softscore") w >> s.softScore;
			else if (k == "messages") w >> s.messages;
			else
				for (int i = 0; i < 9; i++)
					if (k == CONSOLES[i].system)
					{
						SystemStats& y = s.systems[i];
						w >> y.started >> y.completed >> y.mastered >> y.unlocked;
					}
		}
		return s;
	}

	// ---- the games' hashes -----------------------------------------------------------------------------------------
	namespace
	{
		std::mutex hashesMu;	// (the pages' threads: one game's hash kept while another's is read)

		// the file's line's first three fields, "" for one that can't be a line (a tab or a newline in its path)
		std::string hashKey(const std::string& path)
		{
			struct stat sb;
			if (path.find_first_of("\t\n") != std::string::npos || stat(path.c_str(), &sb) != 0) return "";
			return path + "\t" + std::to_string((long long)sb.st_size) + "\t" + std::to_string((long long)sb.st_mtime);
		}
	}

	bool cachedHash(const Card& c, const std::string& path, GameHash& h)
	{
		std::string key = hashKey(path);
		if (key.empty()) return false;
		std::lock_guard<std::mutex> l(hashesMu);
		std::ifstream in(c.hashes);
		std::string ln;
		while (std::getline(in, ln))
		{
			if (ln.size() <= key.size() || ln.compare(0, key.size(), key) != 0 || ln[key.size()] != '\t') continue;
			std::string rest = ln.substr(key.size() + 1);
			size_t tab = rest.find('\t');
			if (tab != 32) continue;
			h.hash = rest.substr(0, 32);
			h.id = (unsigned)strtoul(rest.c_str() + 33, nullptr, 10);
			return true;
		}
		return false;
	}

	bool cacheHash(const Card& c, const std::string& path, const GameHash& h, std::string& error)
	{
		std::string key = hashKey(path);
		if (key.empty() || h.hash.size() != 32) return true;	// (not kept: hashed again next time)
		std::lock_guard<std::mutex> l(hashesMu);
		std::string old, out, ln;
		bool there = false;
		slurp(c.hashes, old, there);
		// the file's earlier lines dropped (the same path, as it was before a change too)
		std::istringstream in(old);
		while (std::getline(in, ln))
			if (ln.compare(0, path.size() + 1, path + "\t") != 0) out += ln + "\n";
		out += key + "\t" + h.hash + "\t" + (h.id ? std::to_string(h.id) : "") + "\n";
		if (!makeDir(c.data + "/cache")) { error = "can't make " + c.data + "/cache: " + strerror(errno); return false; }
		return replace(c.hashes, out, error);
	}
}
