// The RomM helper (kiwami): its log and its requests (http.h). The transport is a copy of raclient.cpp's, with the
// calls a download and an upload need besides.
#include "http.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace banana::kiwami::romm
{
	std::string verb = "romm";

	namespace
	{
		std::string logPath;
		const long LOG_MAX = 256 * 1024;

		struct Curl
		{
			void* (*init)(void) = nullptr;
			int (*setopt)(void*, int, ...) = nullptr;
			int (*perform)(void*) = nullptr;
			int (*getinfo)(void*, int, ...) = nullptr;
			void (*cleanup)(void*) = nullptr;
			const char* (*strerror)(int) = nullptr;
			void* (*slistAppend)(void*, const char*) = nullptr;
			void (*slistFree)(void*) = nullptr;
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
				strerror = (const char* (*)(int))dlsym(l, "curl_easy_strerror");
				slistAppend = (void* (*)(void*, const char*))dlsym(l, "curl_slist_append");
				slistFree = (void (*)(void*))dlsym(l, "curl_slist_free_all");
				ok = init && setopt && perform && getinfo && cleanup && strerror && slistAppend && slistFree;
			}
		};
		const Curl& curl() { static const Curl c; return c; }

		enum
		{
			WRITEDATA = 10001, URL = 10002, POSTFIELDS = 10015, USERAGENT = 10018, LOW_SPEED_LIMIT = 19, LOW_SPEED_TIME = 20,
			HTTPHEADER = 10023, CUSTOMREQUEST = 10036, NOPROGRESS = 43, POST = 47, XFERINFODATA = 10057, NOSIGNAL = 99,
			RANGE = 10007, POSTFIELDSIZE_LARGE = 30120, TIMEOUT_MS = 155, CONNECTTIMEOUT_MS = 156,
			WRITEFUNCTION = 20011, XFERINFOFUNCTION = 20219, RESPONSE_CODE = 0x200002
		};

		// where a request's body goes: a 2xx into the file when there is one, anything else into the string
		struct Sink
		{
			const Curl* cu;
			void* h;
			const Request* r;
			std::string* body;
			FILE* f = nullptr;
			bool decided = false, toFile = false, failed = false;
		};

		size_t onBody(char* p, size_t sz, size_t n, void* u)
		{
			Sink& s = *(Sink*)u;
			if (!s.decided)
			{
				s.decided = true;
				long code = 0;
				s.cu->getinfo(s.h, RESPONSE_CODE, &code);
				if (!s.r->file.empty() && code >= 200 && code < 300)
				{
					// a 206 goes on from the offset asked for; a 200 is the whole file again
					bool append = s.r->resume > 0 && code == 206;
					s.f = fopen(s.r->file.c_str(), append ? "ab" : "wb");
					if (!s.f) { s.failed = true; return 0; }
					s.toFile = true;
				}
			}
			if (!s.toFile) { s.body->append(p, sz * n); return sz * n; }
			if (fwrite(p, sz, n, s.f) != n) { s.failed = true; return 0; }
			return sz * n;
		}

		int onProgress(void* u, long long total, long long now, long long, long long)
		{
			const Request& r = *(const Request*)u;
			if (r.progress) r.progress(now, total);
			return 0;
		}

		// the path and query, for the log: the URL without its scheme and host
		std::string pathOf(const std::string& url)
		{
			size_t s = url.find("://");
			size_t p = url.find('/', s == std::string::npos ? 0 : s + 3);
			return p == std::string::npos ? "/" : url.substr(p);
		}
	}

	void logTo(const std::string& path) { logPath = path; }

	void log(const char* fmt, ...)
	{
		char msg[1024];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(msg, sizeof msg, fmt, ap);
		va_end(ap);
		if (logPath.empty()) { fprintf(stderr, "romm: %s\n", msg); return; }
		struct stat sb;
		if (stat(logPath.c_str(), &sb) == 0 && sb.st_size > LOG_MAX)
		{
			std::string old = logPath.size() > 4 && logPath.compare(logPath.size() - 4, 4, ".log") == 0
				? logPath.substr(0, logPath.size() - 4) + ".1.log" : logPath + ".1";
			rename(logPath.c_str(), old.c_str());
		}
		time_t t = time(nullptr);
		struct tm lt;
		localtime_r(&t, &lt);
		char when[32];
		strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &lt);
		// one write of the whole line, appended: the helpers running at once don't cut into each other's lines
		std::string line = std::string(when) + " [" + std::to_string(getpid()) + "] " + msg + "\n";
		int fd = open(logPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (fd < 0) { fprintf(stderr, "romm: %s", line.c_str()); return; }
		if (write(fd, line.data(), line.size()) < 0) {}
		close(fd);
	}

	Response perform(const Request& r)
	{
		Response res;
		auto t0 = std::chrono::steady_clock::now();
		const Curl& cu = curl();
		void* h = cu.ok ? cu.init() : nullptr;
		if (!h) res.error = cu.ok ? "libcurl failed to start" : "no libcurl";
		else
		{
			void* headers = nullptr;
			std::string auth = "Authorization: Bearer " + r.token, type = "Content-Type: " + r.type;
			headers = cu.slistAppend(headers, "Accept: application/json");
			if (!r.token.empty()) headers = cu.slistAppend(headers, auth.c_str());
			if (!r.type.empty()) headers = cu.slistAppend(headers, type.c_str());
			cu.setopt(h, URL, r.url.c_str());
			cu.setopt(h, HTTPHEADER, headers);
			cu.setopt(h, USERAGENT, "BananaOS romm");
			cu.setopt(h, NOSIGNAL, 1L);
			cu.setopt(h, CONNECTTIMEOUT_MS, r.connectMs);
			if (r.totalMs > 0) cu.setopt(h, TIMEOUT_MS, r.totalMs);
			if (r.lowSpeedSecs > 0) { cu.setopt(h, LOW_SPEED_LIMIT, r.lowSpeedBytes); cu.setopt(h, LOW_SPEED_TIME, r.lowSpeedSecs); }
			if (r.method == "POST" || r.method == "PUT")
			{
				cu.setopt(h, POST, 1L);
				cu.setopt(h, POSTFIELDS, r.body.data());
				cu.setopt(h, POSTFIELDSIZE_LARGE, (long long)r.body.size());
			}
			if (r.method != "GET" && r.method != "POST") cu.setopt(h, CUSTOMREQUEST, r.method.c_str());
			// (a Range of its own, not RESUME_FROM, which fails on a 200: Sink takes that as the whole file again)
			std::string range = std::to_string(r.resume) + "-";
			if (r.resume > 0 || r.range) cu.setopt(h, RANGE, range.c_str());
			if (r.progress)
			{
				cu.setopt(h, NOPROGRESS, 0L);
				cu.setopt(h, XFERINFOFUNCTION, onProgress);
				cu.setopt(h, XFERINFODATA, &r);
			}
			Sink sink{ &cu, h, &r, &res.body };
			cu.setopt(h, WRITEFUNCTION, onBody);
			cu.setopt(h, WRITEDATA, &sink);
			int rc = cu.perform(h);
			if (sink.f && fclose(sink.f) != 0) sink.failed = true;
			if (rc == 0 && !sink.failed) cu.getinfo(h, RESPONSE_CODE, &res.status);
			else
			{
				res.error = sink.failed ? "can't write " + r.file : cu.strerror(rc);
				// what came of the body is kept for a download to go on from; a string's is no answer
				if (!sink.toFile) res.body.clear();
			}
			cu.cleanup(h);
			cu.slistFree(headers);
		}
		res.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		if (res.status) log("%s %s %s %ld %.0f ms", verb.c_str(), r.method.c_str(), pathOf(r.url).c_str(), res.status, res.ms);
		else log("%s %s %s no connection %.0f ms (%s)", verb.c_str(), r.method.c_str(), pathOf(r.url).c_str(), res.ms, res.error.c_str());
		return res;
	}

	std::string multipart(const std::string& field, const std::string& fileName, const std::string& content, std::string& type)
	{
		// a boundary that isn't in the content
		std::string b;
		for (unsigned n = (unsigned)time(nullptr) ^ (unsigned)getpid();; n = n * 1103515245u + 12345u)
		{
			char s[48];
			snprintf(s, sizeof s, "----BananaRomm%08x%08x", n, n * 2654435761u);
			b = s;
			if (content.find(b) == std::string::npos) break;
		}
		type = "multipart/form-data; boundary=" + b;
		std::string name;
		for (char c : fileName) name += c == '"' || c == '\r' || c == '\n' ? '_' : c;
		return "--" + b + "\r\nContent-Disposition: form-data; name=\"" + field + "\"; filename=\"" + name
			+ "\"\r\nContent-Type: application/octet-stream\r\n\r\n" + content + "\r\n--" + b + "--\r\n";
	}
}
