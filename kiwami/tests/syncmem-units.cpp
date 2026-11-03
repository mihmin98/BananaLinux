// banana-sync's RomM listing held small (ui/src/kiwami/syncmem.h), checked on its own: slim()'s copy of a game answers
// every read Romm::manifest() makes exactly as the game RomM sent does, edge cases included, holds nothing else, and
// lets the rest of the page go; and the peak memory a sync's report ends with. Built and run by kiwami/tests/romm-list.sh with the host's g++:
//   g++ -std=c++17 -Wall -Werror -I ui/src kiwami/tests/syncmem-units.cpp ui/src/json.cpp -o syncmem-units
//   ./syncmem-units
#include "kiwami/syncmem.h"
#include <cstdio>
#include <regex>
#include <set>
#include <string>

using banana::JPtr;
using banana::JVal;
using namespace banana::kiwami;

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "syncmem-units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "syncmem-units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

static JPtr parse(const std::string& text)
{
	std::string why;
	JPtr j = banana::parseJson(text, &why);
	if (!j) { fprintf(stderr, "syncmem-units: FAIL can't parse a test's JSON: %s\n", why.c_str()); failed++; }
	return j;
}

// a number as text, so EQ shows both sides
static std::string n(double d) { char b[64]; snprintf(b, sizeof b, "%.17g", d); return b; }

// one member read the three ways manifest() reads members, with and without a default, and for presence
static void sameMember(const JVal& a, const JVal& b, const char* k, const std::string& where)
{
	EQ(b.str(k), a.str(k));
	EQ(b.str(k, "def"), a.str(k, "def"));
	EQ(n(b.num(k)), n(a.num(k)));
	EQ(n(b.num(k, -1)), n(a.num(k, -1)));
	CHECK(b.flag(k) == a.flag(k));
	CHECK(b.flag(k, true) == a.flag(k, true));
	if ((b.get(k) != nullptr) != (a.get(k) != nullptr)) { failed++; fprintf(stderr, "syncmem-units: FAIL %s: \"%s\" present %d, not %d\n", where.c_str(), k, b.get(k) != nullptr, a.get(k) != nullptr); }
	checks++;
}

// every read of manifest()'s, the original against slim()'s copy
static void same(const JVal& game, const std::string& where)
{
	JPtr s = syncmem::slim(game);
	CHECK(s && s->isObj());
	if (!s) return;
	for (const char* k : syncmem::GAME_KEYS) sameMember(game, *s, k, where);
	const JVal* fa = game.get("files");
	const JVal* fb = s->get("files");
	CHECK((fa && fa->isArr()) == (fb && fb->isArr()));
	if (fa && fa->isArr() && fb && fb->isArr())
	{
		CHECK(fa->arr.size() == fb->arr.size());
		for (size_t i = 0; i < fa->arr.size() && i < fb->arr.size(); i++)
			for (const char* k : syncmem::FILE_KEYS) sameMember(*fa->arr[i], *fb->arr[i], k, where + " files[" + std::to_string(i) + "]");
	}
	const JVal* ma = game.get("metadatum");
	const JVal* mb = s->get("metadatum");
	// the gamelist fields manifest() writes from a metadatum, as it writes them (a metadatum that isn't an object, or
	// none, writes nothing); and its members read directly, when both are objects
	auto read = [](const JVal* md) {
		std::string out;
		if (!md) return out;
		if (const JVal* gs = md->get("genres")) if (gs->isArr() && !gs->arr.empty()) out += "genre=" + gs->arr[0]->s + ";";
		double d = md->num("first_release_date", 0);
		if (d > 0) out += "date=" + n(d) + ";";
		std::string pub = md->get("publishers") && md->get("publishers")->isArr() && !md->get("publishers")->arr.empty() ? md->get("publishers")->arr[0]->s : "";
		if (!pub.empty()) out += "pub=" + pub;
		return out;
	};
	EQ(read(mb), read(ma));
	if (ma && mb && ma->isObj()) for (const char* k : syncmem::META_KEYS) sameMember(*ma, *mb, k, where + " metadatum");
}

// the copy's members, at every level, are only the lists'
static void onlyKept(const JVal& s)
{
	std::set<std::string> game(std::begin(syncmem::GAME_KEYS), std::end(syncmem::GAME_KEYS)), file(std::begin(syncmem::FILE_KEYS), std::end(syncmem::FILE_KEYS)),
		meta(std::begin(syncmem::META_KEYS), std::end(syncmem::META_KEYS));
	game.insert({ "files", "metadatum" });
	for (const char* k : syncmem::META_FIRST) meta.insert(k);
	for (auto& m : s.obj)
	{
		if (!game.count(m.first)) { failed++; fprintf(stderr, "syncmem-units: FAIL the copy kept \"%s\"\n", m.first.c_str()); }
		checks++;
	}
	if (const JVal* fs = s.get("files"))
		for (auto& f : fs->arr)
			for (auto& m : f->obj) { checks++; if (!file.count(m.first)) { failed++; fprintf(stderr, "syncmem-units: FAIL the copy kept files[].\"%s\"\n", m.first.c_str()); } }
	if (const JVal* md = s.get("metadatum"))
	{
		for (auto& m : md->obj) { checks++; if (!meta.count(m.first)) { failed++; fprintf(stderr, "syncmem-units: FAIL the copy kept metadatum.\"%s\"\n", m.first.c_str()); } }
		for (const char* k : syncmem::META_FIRST) if (const JVal* a = md->get(k)) CHECK(a->arr.size() <= 1);
	}
}

static const char* RA = R"("merged_ra_metadata": {"first_release_date": 1, "genres": ["Action"], "achievements": [
	{"ra_id": 1, "title": "One", "description": "the first", "points": 5, "num_awarded": 10, "num_awarded_hardcore": 2, "badge_id": "1",
	 "badge_url": "u", "badge_url_lock": "l", "badge_path": "p", "badge_path_lock": "pl", "display_order": 0, "type": null}]},
	"igdb_metadata": {"total_rating": "80"}, "merged_screenshots": ["/a.png", "/b.png"])";

static void testSlim()
{
	// a game as RomM 5.4.1 sends it, with its heavy blocks
	JPtr g = parse(std::string(R"j({"id": 200010, "platform_id": 2, "platform_slug": "gba", "fs_name": "Game 0010 (USA).gba",
		"fs_name_no_ext": "Game 0010 (USA)", "name": "Game 0010", "fs_size_bytes": 11, "has_multiple_files": false,
		"missing_from_fs": false, "summary": "About it.", "path_cover_large": "/assets/romm/resources/roms/2/200010/cover/big.png?ts=2026-10-31 15:44:32.123456+00:00",
		"metadatum": {"rom_id": 200010, "genres": ["Action", "Puzzle"], "publishers": ["Banana", "Peel"], "first_release_date": 1262304000000, "average_rating": 7},
		"files": [{"id": 2000100, "rom_id": 200010, "file_name": "Game 0010 (USA).gba", "file_path": "gba", "file_size_bytes": 11,
			"full_path": "x", "crc_hash": null, "md5_hash": "ABC", "sha1_hash": "DEF", "category": null}], )j") + RA + "}");
	if (!g) return;
	same(*g, "a game");
	JPtr s = syncmem::slim(*g);
	onlyKept(*s);
	EQ(s->str("fs_name"), "Game 0010 (USA).gba");
	CHECK(!s->get("merged_ra_metadata") && !s->get("igdb_metadata") && !s->get("merged_screenshots") && !s->get("platform_slug"));
	// (fs_name_no_ext is read by list() from the page itself, not by manifest())
	CHECK(!s->get("fs_name_no_ext"));
	CHECK(s->get("metadatum")->get("genres")->arr.size() == 1 && s->get("metadatum")->get("publishers")->arr.size() == 1);

	// the edge cases: nulls, a number for a name, a release date in seconds, scalars given as arrays and objects (the
	// copy holds a null in their place), files of two (one not an object), a numeric first genre, empty publishers
	JPtr e = parse(R"({"id": "300007", "platform_id": 3.0, "fs_name": ["not", "a", "name"], "name": 1942, "fs_size_bytes": null,
		"has_multiple_files": "yes", "missing_from_fs": 1, "summary": null, "path_cover_large": {"url": "x"},
		"metadatum": {"genres": [7, "Action"], "publishers": [], "first_release_date": "1262304000"},
		"files": [{"id": 1, "file_name": "a.cue", "file_size_bytes": null, "sha1_hash": ["x"], "md5_hash": {"y": 1}}, "not an object", 5]})");
	if (!e) return;
	same(*e, "the edge cases");
	JPtr se = syncmem::slim(*e);
	onlyKept(*se);
	CHECK(se->get("summary") && se->get("summary")->kind == JVal::NUL);
	CHECK(se->get("path_cover_large") && se->get("path_cover_large")->kind == JVal::NUL);
	CHECK(se->get("files")->arr.size() == 3 && se->get("files")->arr[1]->isObj() && se->get("files")->arr[1]->obj.empty());
	CHECK(se->get("files")->arr[0]->get("file_size_bytes") != nullptr);	// present (null): manifest() takes its 0
	EQ(se->get("metadatum")->get("genres")->arr[0]->s, "");
	CHECK(se->get("metadatum")->get("publishers")->isArr() && se->get("metadatum")->get("publishers")->arr.empty());

	// none of it there: no metadatum, files not an array, genres and publishers not arrays, a metadatum not an object
	for (const char* t : { R"({"id": 1})", R"({"files": {"id": 1}, "metadatum": {"genres": "Action", "publishers": "Banana"}})",
		R"({"files": [], "metadatum": [1, 2]})", R"({"files": null, "metadatum": null, "missing_from_fs": true})", "{}" })
	{
		JPtr j = parse(t);
		if (!j) continue;
		same(*j, t);
		onlyKept(*syncmem::slim(*j));
	}
	JPtr ne = parse(R"({"files": {"id": 1}, "metadatum": [1, 2]})");
	if (ne) { JPtr sne = syncmem::slim(*ne); CHECK(!sne->get("files") && !sne->get("metadatum")); }

	// a game that isn't an object gives an empty object
	for (const char* t : { "[1, 2]", "null", "\"game\"", "42" })
	{
		JPtr j = parse(t);
		if (!j) continue;
		JPtr sj = syncmem::slim(*j);
		CHECK(sj->isObj() && sj->obj.empty());
	}

	// two members of one name (the parser keeps the last; one built by hand can hold both): the first, as get() finds it
	JVal d;
	d.kind = JVal::OBJ;
	auto str = [](const char* v) { auto p = std::make_shared<JVal>(); p->kind = JVal::STR; p->s = v; return JPtr(p); };
	d.obj.emplace_back("name", str("First"));
	d.obj.emplace_back("name", str("Second"));
	d.obj.emplace_back("fs_name", str("A (USA).gba"));
	same(d, "two names");
	EQ(syncmem::slim(d)->str("name"), "First");
	CHECK(syncmem::slim(d)->obj.size() == 2);
}

// the page goes once its games are slimmed: the heavy blocks and the page itself are freed, the kept copy holds on
// to the scalars it shares
static void testFreed()
{
	JPtr page = parse(std::string(R"j({"items": [{"id": 1, "platform_id": 2, "fs_name": "A (USA).gba", "summary": "Kept.",
		"metadatum": {"genres": ["Action"]}, "files": [{"id": 10, "file_name": "A (USA).gba"}], )j") + RA + "}], \"total\": 1}");
	if (!page) return;
	const JVal* item = page->get("items")->arr[0].get();
	std::weak_ptr<const JVal> shots = item->obj.back().second;	// merged_screenshots
	std::weak_ptr<const JVal> raMeta;
	for (auto& m : item->obj) if (m.first == "merged_ra_metadata") raMeta = m.second;
	std::weak_ptr<const JVal> items = page->obj[0].second, whole = page;
	JPtr kept = syncmem::slim(*item);
	CHECK(!raMeta.expired() && !shots.expired());
	page.reset();
	CHECK(whole.expired());
	CHECK(items.expired());
	CHECK(raMeta.expired());
	CHECK(shots.expired());
	EQ(kept->str("summary"), "Kept.");
	EQ(kept->get("files")->arr[0]->str("file_name"), "A (USA).gba");
	EQ(kept->get("metadatum")->get("genres")->arr[0]->s, "Action");
}

// the peak memory line: VmHWM's number from /proc/<pid>/status, "unknown" without one
static void testPeak()
{
	EQ(syncmem::peakFrom("VmPeak:\t  9000 kB\nVmHWM:\t    2345 kB\nVmRSS:\t 100 kB\n"), "2345 kB");
	EQ(syncmem::peakFrom("VmHWM:\t 77 kB"), "77 kB");
	EQ(syncmem::peakFrom("VmRSS:\t 100 kB\n"), "unknown");
	EQ(syncmem::peakFrom("VmHWM:\n"), "unknown");
	EQ(syncmem::peakFrom("VmHWM:\nVmRSS:\t 100 kB\n"), "unknown");
	EQ(syncmem::peakFrom(""), "unknown");
	std::string p = syncmem::peak();
	if (!std::regex_match(p, std::regex("[0-9]+ kB"))) { failed++; fprintf(stderr, "syncmem-units: FAIL peak() on this host is \"%s\"\n", p.c_str()); }
	checks++;
}

int main()
{
	testSlim();
	testFreed();
	testPeak();
	if (failed) { fprintf(stderr, "syncmem-units: %d of %d checks failed\n", failed, checks); return 1; }
	printf("syncmem-units: %d checks passed\n", checks);
	return 0;
}
