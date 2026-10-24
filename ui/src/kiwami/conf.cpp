// banana-ui (kiwami): key=value files on the card (conf.h).
#include "conf.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace banana::kiwami
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
	}

	std::string confGet(const std::string& path, const std::string& key)
	{
		std::ifstream f(path);
		std::string ln, v, r;
		while (std::getline(f, ln))
			if (keyed(ln, key, &v)) r = v;
		return r;
	}

	bool readFile(const std::string& path, std::string& s, bool& there)
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

	bool replaceFile(const std::string& path, const std::string& text, std::string& error)
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
		return false;
	}

	bool confEdit(const std::string& path, const std::vector<ConfEdit>& edits, const char* sep, bool create, std::string& error)
	{
		std::string old;
		bool there = false;
		if (!readFile(path, old, there) && there)
		{
			error = "can't read " + path;
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
		return replaceFile(path, out, error);
	}
}
