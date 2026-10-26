#pragma once
// The RomM helper (kiwami): its log and its requests. libcurl is dlopen()ed (libcurl.so.4) as banana-ui's
// RetroAchievements client and upstream's account check do: the image has it, nothing links it. Every request logs
// one line,
//   <verb> <METHOD> <path?query> <HTTP status | no connection> <ms> ms
// and nothing else of it: never a header (the token is one) or a body.
#include <functional>
#include <string>

namespace banana::kiwami::romm
{
	// the log: lines appended to path, each with the time in the menu's time zone (TZ, which main sets), the file
	// moved to <path without .log>.1.log past 256 KB. Until logTo, lines go to stderr
	void logTo(const std::string& path);
	void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
	extern std::string verb;	// the verb running, first on each request's line

	struct Request
	{
		std::string method = "GET", url;
		std::string token;					// Authorization: Bearer, when not ""
		std::string body, type;				// a body (POST, PUT) and its Content-Type
		long connectMs = 5000, totalMs = 20000;	// 0: no limit on the whole
		// the body written to file instead of returned (for a 2xx; another status's body is returned), from offset
		// resume on when it is more than 0 and the server answers 206, else from the start
		std::string file;
		long long resume = 0;
		bool range = false;		// a Range sent from resume even when that is 0
		long lowSpeedBytes = 0, lowSpeedSecs = 0;	// given up after lowSpeedSecs under lowSpeedBytes a second
		std::function<void(long long got, long long total)> progress;	// bytes so far and expected (0: unknown)
	};
	struct Response
	{
		long status = 0;		// 0: no answer
		std::string body, error;
		double ms = 0;
	};
	Response perform(const Request& r);

	// a multipart/form-data body with one file field; type is set to its Content-Type, with the boundary
	std::string multipart(const std::string& field, const std::string& fileName, const std::string& content, std::string& type);
}
