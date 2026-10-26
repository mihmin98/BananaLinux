// The RomM helper (kiwami): games and BIOS files from RomM onto the cards (download.h).
#include "download.h"
#include "http.h"
#include "../third_party/rcheevos/src/rhash/md5.h"
#include <cerrno>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

namespace banana::kiwami::romm
{
	namespace
	{
		std::string firstLine(const std::string& path)
		{
			std::ifstream f(path);
			std::string ln;
			std::getline(f, ln);
			return ln;
		}

		long long sizeOf(const std::string& path)
		{
			struct stat st;
			return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) ? (long long)st.st_size : 0;
		}

		bool isDir(const std::string& path)
		{
			struct stat st;
			return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
		}
	}

	std::string sd2Roms(const Card& c)
	{
		std::string sd2 = firstLine(c.run + "/sd2/roms");
		while (!sd2.empty() && (sd2.back() == ' ' || sd2.back() == '/' || sd2.back() == '\r')) sd2.pop_back();
		return sd2;
	}

	Target target(const Card& c)
	{
		Target t;
		t.roms = c.card + "/roms";
		if (setting(c, "romm_target", "main") != "sd2") return t;
		std::string sd2 = sd2Roms(c);
		if (!sd2.empty() && isDir(sd2)) t.roms = sd2;
		else t.fellBack = true;
		return t;
	}

	std::string folderOn(const std::string& roms, const System& s)
	{
		return !s.folder.empty() && s.folder[0] == '/' ? s.folder : roms + "/" + s.folder;
	}

	bool safeName(const std::string& n)
	{
		return !n.empty() && n[0] != '.' && n.find('/') == std::string::npos && n.find('\0') == std::string::npos;
	}

	bool safePath(const std::string& p)
	{
		size_t from = 0;
		while (true)
		{
			size_t sl = p.find('/', from);
			if (!safeName(p.substr(from, sl == std::string::npos ? std::string::npos : sl - from))) return false;
			if (sl == std::string::npos) return true;
			from = sl + 1;
		}
	}

	long long freeBytes(const std::string& path)
	{
		const char* e = getenv("KIWAMI_ROMM_FREE");
		if (e && *e) return atoll(e);
		std::string p = path;
		struct statvfs v;
		while (statvfs(p.c_str(), &v) != 0)
		{
			size_t sl = p.rfind('/');
			if (sl == std::string::npos || p.size() <= 1) return -1;
			p = sl ? p.substr(0, sl) : "/";
		}
		return (long long)v.f_bavail * (long long)v.f_frsize;
	}

	Awake::Awake(const Card& c) : path(c.run + "/awake.pid")
	{
		pid_t other = (pid_t)atol(firstLine(path).c_str());
		if (other > 0 && other != getpid() && kill(other, 0) == 0)
		{
			log("%s: awake.pid is %d's, left alone (the handheld may sleep when idle)", verb.c_str(), (int)other);
			return;
		}
		FILE* f = fopen(path.c_str(), "w");
		if (!f) { log("%s: can't write %s", verb.c_str(), path.c_str()); return; }
		fprintf(f, "%d\n", (int)getpid());
		held = fclose(f) == 0;
		if (held) log("%s: awake.pid taken: no idle sleep until the end", verb.c_str());
	}

	Awake::~Awake()
	{
		if (!held || atol(firstLine(path).c_str()) != getpid()) return;
		unlink(path.c_str());
		log("%s: awake.pid let go", verb.c_str());
	}

	Transfer transfer(const std::string& part, long long expected, const std::function<Response(long long from)>& get)
	{
		Transfer t;
		long long have = sizeOf(part);
		if (expected > 0 && have > expected) { unlink(part.c_str()); have = 0; }
		if (expected > 0 && have == expected)
		{
			log("%s: %s: already whole", verb.c_str(), part.c_str());
			t.ok = true;
			t.status = 200;
			return t;
		}
		if (have) log("%s: resumed from %lld bytes", verb.c_str(), have);
		Response r = get(have);
		if (r.status == 416 && have)
		{
			// (what part has isn't a beginning of this file: from the start)
			unlink(part.c_str());
			r = get(have = 0);
		}
		t.status = r.status;
		if (r.status == 200 && have) log("%s: RomM sent the whole file again: started over", verb.c_str());
		if (r.status == 200 || r.status == 206)
		{
			long long got = sizeOf(part);
			if (expected > 0 && got != expected)
			{
				log("%s: %lld bytes came, not %lld: dropped", verb.c_str(), got, expected);
				unlink(part.c_str());
				t.why = "size";
				return t;
			}
			t.ok = true;
			return t;
		}
		if (!r.status) t.why = sizeOf(part) > have ? "cut" : "down";
		else if (r.status == 404) t.why = "notfound";
		else t.why = "http " + std::to_string(r.status);
		return t;
	}

	bool unpack(const std::string& zip, const std::string& dir, std::string& error)
	{
		if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) { error = "can't make " + dir + ": " + strerror(errno); return false; }
		pid_t p = fork();
		if (p < 0) { error = std::string("can't run unzip: ") + strerror(errno); return false; }
		if (p == 0)
		{
			int fd = open("/dev/null", O_RDWR);
			if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); }
			execlp("unzip", "unzip", "-q", "-o", zip.c_str(), "-d", dir.c_str(), (char*)nullptr);
			_exit(127);
		}
		int st = 0;
		while (waitpid(p, &st, 0) < 0 && errno == EINTR) {}
		if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return true;
		error = WIFEXITED(st) && WEXITSTATUS(st) == 127 ? "no unzip" : "unzip failed (" + std::to_string(WIFEXITED(st) ? WEXITSTATUS(st) : -1) + ")";
		return false;
	}

	bool removeTree(const std::string& dir)
	{
		if (access(dir.c_str(), F_OK) != 0) return true;
		return nftw(dir.c_str(), [](const char* p, const struct stat*, int, struct FTW*) { return ::remove(p); }, 16, FTW_DEPTH | FTW_PHYS) == 0;
	}

	std::string fileMd5(const std::string& path)
	{
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) return "";
		md5_state_t st;
		md5_byte_t d[16];
		md5_init(&st);
		char buf[65536];
		size_t n;
		while ((n = fread(buf, 1, sizeof buf, f)) > 0) md5_append(&st, (const md5_byte_t*)buf, (int)n);
		bool bad = ferror(f);
		fclose(f);
		if (bad) return "";
		md5_finish(&st, d);
		static const char* HEX = "0123456789abcdef";
		std::string h;
		for (auto b : d) { h += HEX[b >> 4]; h += HEX[b & 15]; }
		return h;
	}
}
