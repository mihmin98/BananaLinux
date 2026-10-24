// The RomM helper (kiwami): the games' saves on the card (saves.h).
#include "saves.h"
#include "http.h"
#include "../conf.h"
#include "../third_party/rcheevos/src/rhash/md5.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace banana::kiwami::romm
{
	const Synced SYNCED[9] = {
		{ "nds", { "nds", nullptr } }, { "gba", { "gba", nullptr } }, { "gbc", { "gbc", nullptr } }, { "gb", { "gb", nullptr } },
		{ "nes", { "nes", "famicom" } }, { "snes", { "snes", "sfam" } }, { "megadrive", { "genesis", nullptr } },
		{ "psx", { "psx", nullptr } }, { "n64", { "n64", nullptr } },
	};

	namespace
	{
		// the cores' library_name, the folder RetroArch keeps their saves and states in
		const char* const LIBRARY_NAME[][2] = {
			{ "gpsp", "gpSP" }, { "mgba", "mGBA" }, { "gambatte", "Gambatte" }, { "sameboy", "SameBoy" }, { "fceumm", "FCEUmm" },
			{ "mesen", "Mesen" }, { "snes9x", "Snes9x" }, { "picodrive", "PicoDrive" }, { "genesis_plus_gx", "Genesis Plus GX" },
			{ "mupen64plus_next", "Mupen64Plus-Next" }, { "pcsx_rearmed", "PCSX-ReARMed" },
		};

		std::string trim(const std::string& s)
		{
			size_t b = s.find_first_not_of(" \t\r"), e = s.find_last_not_of(" \t\r");
			return b == std::string::npos ? "" : s.substr(b, e - b + 1);
		}

		std::string dirOf(const std::string& path)
		{
			size_t s = path.rfind('/');
			return s == std::string::npos ? "." : path.substr(0, s);
		}

		bool exists(const std::string& path) { return access(path.c_str(), F_OK) == 0; }

		// bytes into path: through <path>.new (synced) and a rename, the file there before kept as <path>.bak
		bool writeKeeping(const std::string& path, const std::string& bytes, std::string& error)
		{
			if (!makeDir(dirOf(path))) { error = "can't make " + dirOf(path) + ": " + strerror(errno); return false; }
			std::string tmp = path + ".new";
			int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			bool ok = fd >= 0 && write(fd, bytes.data(), bytes.size()) == (ssize_t)bytes.size() && fsync(fd) == 0;
			int err = errno;
			if (fd >= 0 && close(fd) != 0 && ok) { ok = false; err = errno; }
			if (ok && exists(path) && rename(path.c_str(), (path + ".bak").c_str()) != 0) { ok = false; err = errno; }
			if (ok && rename(tmp.c_str(), path.c_str()) != 0) { ok = false; err = errno; }
			if (ok) return true;
			unlink(tmp.c_str());
			error = "can't write " + path + ": " + strerror(err);
			return false;
		}
	}

	std::vector<System> parseSystems(const std::string& text)
	{
		std::vector<System> all;
		std::istringstream in(text);
		std::string ln;
		while (std::getline(in, ln))
		{
			ln = trim(ln);
			if (ln.empty() || ln[0] == '#') continue;
			if (ln[0] == '[' && ln.back() == ']')
			{
				System s;
				s.id = s.folder = ln.substr(1, ln.size() - 2);
				all.push_back(s);
				continue;
			}
			size_t eq = ln.find('=');
			if (all.empty() || eq == std::string::npos) continue;
			std::string k = trim(ln.substr(0, eq)), v = trim(ln.substr(eq + 1));
			System& s = all.back();
			if (k == "folder") s.folder = v;
			else if (k == "run") s.run = v;
			else if (k == "flat") s.flat = v == "yes";
			else if (k == "ext")
			{
				s.ext.clear();
				std::istringstream w(v);
				std::string e;
				while (w >> e) s.ext.push_back(e);
			}
		}
		return all;
	}

	std::vector<System> systems(const Card& c)
	{
		std::string text;
		bool there = false;
		if (!readFile(c.data + "/systems.conf", text, there) || text.empty()) readFile(c.etc + "/systems.conf", text, there);
		return parseSystems(text);
	}

	const System* findSystem(const std::vector<System>& all, const std::string& id)
	{
		for (auto& s : all) if (s.id == id) return &s;
		return nullptr;
	}

	const Synced* synced(const std::string& system)
	{
		for (auto& s : SYNCED) if (system == s.system) return &s;
		return nullptr;
	}

	std::string coreOf(const Card& c, const System& s)
	{
		if (s.run.find("/run-nds ") != std::string::npos) return "nds";
		size_t ra = s.run.find("/run-retroarch ");
		bool ps1 = s.run.find("/run-ps1 ") != std::string::npos;
		if (ra == std::string::npos && !ps1) return "";
		// run-retroarch's choice: core.<system> when that core is installed (run-ps1 hands over to it too)
		std::string chosen = setting(c, "core." + s.id, "");
		if (!chosen.empty() && exists(c.libretro + "/" + chosen + "_libretro.so")) return chosen;
		if (ps1) return "pcsx_rearmed";
		std::istringstream w(s.run.substr(ra + 15));
		std::string core;
		w >> core;
		return core;
	}

	const char* libraryName(const std::string& core)
	{
		for (auto& l : LIBRARY_NAME) if (core == l[0]) return l[1];
		return nullptr;
	}

	std::string stemOf(const std::string& path)
	{
		size_t s = path.rfind('/');
		std::string name = s == std::string::npos ? path : path.substr(s + 1);
		size_t d = name.rfind('.');
		return d == std::string::npos || d == 0 ? name : name.substr(0, d);
	}

	bool saveOf(const Card& c, const System& s, const std::string& rom, Save& out, std::string& why)
	{
		out = Save();
		out.system = s.id;
		out.rom = rom;
		out.stem = stemOf(rom);
		std::string core = coreOf(c, s);
		if (core == "nds")
		{
			out.ds = true;
			out.path = c.card + "/saves/nds/" + out.stem + ".sav";
			out.dsv = c.card + "/drastic/backup/" + out.stem + ".dsv";
			out.emulator = "melonds";
			return true;
		}
		const char* lib = libraryName(core);
		if (!lib) { why = core.empty() ? "no core in its run line" : "the core " + core + " isn't one the helper knows"; return false; }
		out.path = c.card + "/saves/" + lib + "/" + out.stem + ".srm";
		out.state = c.card + "/states/" + lib + "/" + out.stem + ".state.auto";
		out.emulator = core;
		return true;
	}

	void ndsSaves(const Card& c, const std::string& rom)
	{
		int fds[2];
		if (pipe(fds) != 0) return;
		pid_t p = fork();
		if (p < 0) { close(fds[0]); close(fds[1]); return; }
		if (p == 0)
		{
			dup2(fds[1], 1);
			dup2(fds[1], 2);
			close(fds[0]);
			close(fds[1]);
			execl(c.ndsSaves.c_str(), c.ndsSaves.c_str(), rom.c_str(), (char*)nullptr);
			_exit(127);
		}
		close(fds[1]);
		std::string out;
		char buf[512];
		for (ssize_t n; (n = read(fds[0], buf, sizeof buf)) > 0;) out.append(buf, n);
		close(fds[0]);
		int st = 0;
		waitpid(p, &st, 0);
		std::istringstream in(out);
		std::string ln;
		while (std::getline(in, ln)) if (!ln.empty()) log("%s: %s", verb.c_str(), ln.c_str());
		if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) log("%s: nds-saves failed (%d)", verb.c_str(), WIFEXITED(st) ? WEXITSTATUS(st) : -1);
	}

	bool readSave(const Save& s, std::string& bytes, long long& mtimeMs)
	{
		struct stat sb;
		bool there = false;
		if (stat(s.path.c_str(), &sb) != 0 || !S_ISREG(sb.st_mode) || !readFile(s.path, bytes, there)) return false;
		mtimeMs = (long long)sb.st_mtim.tv_sec * 1000 + sb.st_mtim.tv_nsec / 1000000;
		return true;
	}

	bool replaceSave(const Card& c, const Save& s, const std::string& bytes, std::string& error)
	{
		if (s.ds)
		{
			if (!writeKeeping(s.dsv, bytes, error)) return false;
			// BananaDS's save out of the way (kept as .bak), so nds-saves makes it again from DraStic's whatever the two
			// files' times (its -nt goes by the second)
			if (exists(s.path) && rename(s.path.c_str(), (s.path + ".bak").c_str()) != 0)
				log("%s: %s: BananaDS's save couldn't be renamed: %s", verb.c_str(), s.stem.c_str(), strerror(errno));
			ndsSaves(c, s.rom);
			log("%s: %s: RomM's save written as DraStic's (%zu bytes), the one before kept as .bak", verb.c_str(), s.stem.c_str(), bytes.size());
			return true;
		}
		if (!writeKeeping(s.path, bytes, error)) return false;
		log("%s: %s: RomM's save in place (%zu bytes), the one before kept as .bak", verb.c_str(), s.stem.c_str(), bytes.size());
		if (exists(s.state))
		{
			if (rename(s.state.c_str(), (s.state + ".bak").c_str()) == 0)
				log("%s: %s: the Quick resume state renamed .state.auto.bak, so the game starts from RomM's save", verb.c_str(), s.stem.c_str());
			else log("%s: %s: the Quick resume state couldn't be renamed: %s", verb.c_str(), s.stem.c_str(), strerror(errno));
		}
		return true;
	}

	bool dropSave(const Save& s, std::string& error)
	{
		for (const std::string& f : { s.path, s.dsv })
		{
			if (f.empty() || !exists(f)) continue;
			if (rename(f.c_str(), (f + ".bak").c_str()) != 0) { error = "can't rename " + f + ": " + strerror(errno); return false; }
			log("%s: %s: emptied on RomM, the card's save renamed %s.bak", verb.c_str(), s.stem.c_str(), f.substr(f.rfind('/') + 1).c_str());
		}
		return true;
	}

	std::string md5Hex(const std::string& bytes)
	{
		md5_state_t st;
		md5_byte_t d[16];
		md5_init(&st);
		// (md5_append takes an int: a save bigger than that is fed in pieces)
		for (size_t at = 0; at < bytes.size(); at += 1 << 30)
			md5_append(&st, (const md5_byte_t*)bytes.data() + at, (int)std::min(bytes.size() - at, (size_t)1 << 30));
		md5_finish(&st, d);
		static const char* HEX = "0123456789abcdef";
		std::string h;
		for (auto b : d) { h += HEX[b >> 4]; h += HEX[b & 15]; }
		return h;
	}
}
