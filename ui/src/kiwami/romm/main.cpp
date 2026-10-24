// The RomM helper (kiwami), /usr/libexec/kiwami/romm: everything of the fork's RomM client that talks to the server,
// for Settings' RomM pages and the wrapper around the games (romm-game). One verb a run:
//   romm check     one line to stdout: ok <version>, newer <version>, old <version>, unpaired, revoked or down;
//                  exits 0 for ok and newer
//   romm pair      pairs with accounts.conf's romm_url by RomM's device flow: prints (and puts in the status file)
//                  "pairing <code>\t<the page to approve it on>", then goes on in the background, asking RomM every
//                  few seconds until the user approves it (the token and the device written to accounts.conf, the
//                  status "paired <version>"), denies it or lets it expire ("pair-failed denied|expired"). A second
//                  pair stops the first. Failing at once, it prints the pair-failed line and exits 1
//   romm logout    asks RomM to delete the device (which takes its token away; skipped with no server), then takes the
//                  token and the device out of accounts.conf
// The status file is <run>/romm.status, one line; the log <card>/banana/logs/romm.log (http.h), which the token never
// reaches: the helper reads it from accounts.conf, never from its command line. The paths: rommcard.h.
// KIWAMI_ROMM_SYNC=1 keeps it in the foreground (the tests); KIWAMI_MODEL moves /proc/device-tree/model.
#include "api.h"
#include "http.h"
#include "../conf.h"
#include "../rommcard.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace banana::kiwami;
using namespace banana::kiwami::romm;

namespace
{
	Card card;

	std::string firstLine(const std::string& path)
	{
		std::ifstream f(path);
		std::string ln;
		std::getline(f, ln);
		std::string o;
		for (char c : ln) if (c != '\0' && c != '\r') o += c;
		return o;
	}

	void status(const std::string& line)
	{
		std::string error;
		makeDir(card.run);
		if (!replaceFile(card.status, line + "\n", error)) log("%s", error.c_str());
	}

	// a line to stdout for the caller (Settings' job, the tests), and into the log
	void say(const std::string& line)
	{
		printf("%s\n", line.c_str());
		fflush(stdout);
		log("%s: %s", verb.c_str(), line.c_str());
	}

	bool sync() { const char* e = getenv("KIWAMI_ROMM_SYNC"); return e && strcmp(e, "1") == 0; }

	// into the background, out of the caller's session, its pipes let go: true in the child, which goes on; the parent
	// exits. With KIWAMI_ROMM_SYNC=1, nothing: the work goes on in the foreground
	bool detach()
	{
		if (sync()) return true;
		fflush(stdout);
		pid_t p = fork();
		if (p < 0) return true;
		if (p > 0) _exit(0);
		setsid();
		int fd = open("/dev/null", O_RDWR);
		if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
		return true;
	}

	std::string deviceName()
	{
		const char* e = getenv("KIWAMI_MODEL");
		std::string model = firstLine(e && *e ? e : "/proc/device-tree/model");
		return model.empty() ? "BananaOS" : "BananaOS on " + model;
	}

	Server server(const Account& a, long connectMs, long totalMs)
	{
		Server s;
		s.url = a.url;
		s.token = a.token;
		s.connectMs = connectMs;
		s.totalMs = totalMs;
		return s;
	}

	// the server's version, checked: false (and the reason in why: down, or old <v>) when it can't be synced with
	bool versionChecked(const Server& s, std::string& version, std::string& why)
	{
		Heartbeat h = heartbeat(s);
		if (!h.ok) { why = "down"; log("%s: no server (%s)", verb.c_str(), h.error.c_str()); return false; }
		version = h.version;
		Version v = versionOk(h.version);
		if (v == OLD) { why = "old " + h.version; log("%s: RomM %s is older than 5.4.0, refused", verb.c_str(), h.version.c_str()); return false; }
		if (v == NEWER) log("%s: RomM %s is newer than 5.4, which this client was written for", verb.c_str(), h.version.c_str());
		return true;
	}

	// ---- pairing ---------------------------------------------------------------------------------------------------
	std::string pairPid() { return card.run + "/romm-pair.pid"; }

	// a pairing still waiting, stopped (its pid file names it, and it is this helper's pair)
	void stopPairing()
	{
		pid_t p = (pid_t)atol(firstLine(pairPid()).c_str());
		if (p <= 0 || p == getpid()) return;
		std::ifstream f("/proc/" + std::to_string(p) + "/cmdline", std::ios::binary);
		std::stringstream ss;
		ss << f.rdbuf();
		std::string cmd = ss.str();
		size_t a1 = cmd.find('\0');
		if (a1 == std::string::npos || cmd.compare(a1 + 1, 5, std::string("pair\0", 5)) != 0) return;
		std::string arg0 = cmd.substr(0, a1);
		if (arg0.size() < 4 || arg0.compare(arg0.size() - 4, 4, "romm") != 0) return;
		if (kill(p, SIGTERM) != 0) return;
		for (int i = 0; i < 50 && kill(p, 0) == 0; i++) usleep(20000);
		log("%s: the pairing waiting before (%d) stopped", verb.c_str(), (int)p);
	}

	int pair()
	{
		Account a = account(card);
		if (a.url.empty()) { status("pair-failed noserver"); say("pair-failed noserver"); return 1; }
		stopPairing();
		Server s = server(a, 3000, 10000);
		s.token.clear();
		std::string version, why;
		if (!versionChecked(s, version, why)) { status("pair-failed " + why); say("pair-failed " + why); return 1; }
		std::string id = deviceIdentifier(card);
		if (id.empty()) { status("pair-failed card"); say("pair-failed card"); return 1; }
		DeviceInit d = deviceInit(s, deviceName(), id, firstLine(card.etc + "/version"));
		if (!d.ok)
		{
			log("pair: refused (%s)", d.error.c_str());
			status("pair-failed down");
			say("pair-failed down");
			return 1;
		}
		std::string line = "pairing " + d.userCode + "\t" + a.url + d.path;
		status(line);
		say(line);
		detach();
		{
			std::string error;
			if (!replaceFile(pairPid(), std::to_string(getpid()) + "\n", error)) log("pair: %s", error.c_str());
		}
		time_t deadline = time(nullptr) + d.expiresIn;
		int interval = d.interval < 1 ? 1 : d.interval;
		std::string result;
		while (result.empty())
		{
			sleep(interval);
			if (time(nullptr) > deadline) { result = "pair-failed expired"; break; }
			DeviceToken t = deviceToken(s, d.deviceCode);
			switch (t.state)
			{
			case DeviceToken::GRANTED:
			{
				std::string error;
				if (paired(card, t.token, t.deviceId, error)) result = "paired " + version;
				else { log("pair: %s", error.c_str()); result = "pair-failed card"; }
				break;
			}
			case DeviceToken::SLOW_DOWN: interval += 5; break;
			case DeviceToken::DENIED: result = "pair-failed denied"; break;
			case DeviceToken::EXPIRED: result = "pair-failed expired"; break;
			case DeviceToken::PENDING: break;
			case DeviceToken::FAILED: break;	// (the network back in a moment: asked again until it expires)
			}
		}
		status(result);
		log("pair: %s", result.c_str());
		if (atol(firstLine(pairPid()).c_str()) == getpid()) unlink(pairPid().c_str());
		return result.compare(0, 7, "paired ") == 0 ? 0 : 1;
	}

	// ---- the rest ------------------------------------------------------------------------------------------------
	int check()
	{
		Account a = account(card);
		if (a.url.empty()) { say("unpaired"); return 1; }
		Server s = server(a, 3000, 8000);
		std::string version, why;
		if (!versionChecked(s, version, why)) { say(why); return 1; }
		if (a.token.empty() || a.deviceId.empty()) { say("unpaired"); return 1; }
		long st = getDevice(s, a.deviceId);
		if (st == 401 || st == 403 || st == 404) { say("revoked"); return 1; }
		if (st != 200) { say("down"); return 1; }
		say((versionOk(version) == NEWER ? "newer " : "ok ") + version);
		return 0;
	}

	int logout()
	{
		stopPairing();
		Account a = account(card);
		if (!a.url.empty() && !a.token.empty() && !a.deviceId.empty())
		{
			long st = deleteDevice(server(a, 1500, 5000), a.deviceId);
			if (st == 200 || st == 204 || st == 404) log("logout: the device is off RomM");
			else if (st == 401) log("logout: RomM no longer took the token");
			else log("logout: the device couldn't be deleted on RomM (%s), forgotten here all the same",
				st ? std::to_string(st).c_str() : "no server");
		}
		std::string error;
		if (!forget(card, error)) { log("logout: %s", error.c_str()); say("logout-failed card"); return 1; }
		status("unpaired");
		say("logged-out");
		return 0;
	}
}

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);
	std::string tz = setting(card, "timezone", "");
	if (!tz.empty()) { setenv("TZ", tz.c_str(), 1); tzset(); }
	makeDir(card.logs);
	logTo(card.logs + "/romm.log");
	verb = argc > 1 ? argv[1] : "";
	if (verb == "check") return check();
	if (verb == "pair") return pair();
	if (verb == "logout") return logout();
	fprintf(stderr, "usage: romm check | pair | logout\n");
	return 2;
}
