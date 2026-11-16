#pragma once
// banana-sync (kiwami): BIOS files from RomM's firmware (romm.h's kiwami bios verbs), for Home server > BIOS.
// RomM 5.4.1 (backend/endpoints/firmware.py) lists a platform's files at GET /api/firmware?platform_id=<id> and one at
// GET /api/firmware/<id> (id, platform_id, file_name, file_size_bytes, md5_hash, sha1_hash, missing_from_fs, ...), and
// serves it at GET /api/firmware/<id>/content/<file name>, each under the token's firmware.read scope (a 403 without
// it: a pairing made before the fork asked for it, which has to pair again).
// A file goes to /data/bios/<its name>, where RetroArch's cores and BananaDS read their BIOS. The DS's bios7.bin,
// bios9.bin and firmware.bin (any case) also go to DraStic's own folder under its own names, nds_bios_arm7.bin,
// nds_bios_arm9.bin and nds_firmware.bin in /data/drastic/system/ (the names in its binary; run-drastic runs it from
// /data/drastic). A file already there is compared by RomM's MD5 (its SHA-1, else its size, when RomM gives none):
// the same is left, and a different one is never written over, only said. A download goes through <file>.part
// (upstream's fetchTo()), checked against the hash before it takes the file's name; one that fails leaves nothing.
//   kiwami bios list: "<id> TAB <system> TAB <file name> TAB <bytes> TAB missing|same|different" a file, then
//     "ok <n>". missing: a place it goes to has nothing; different: a different file is in one (kept); same: all hold it.
//   kiwami bios get <id>...: "<id> TAB <path under /data> TAB got|same|kept|failed <why>" a place, then
//     "ok <files got> <files all there already> <files with a different one kept>", or "error <why>" when one failed.
#include "romm.h"

namespace banana::kiwami::romm::bios
{
	enum State { MISSING, SAME, DIFFERENT };
	inline const char* stateName(State s) { return s == MISSING ? "missing" : s == SAME ? "same" : "different"; }

	// one of RomM's firmware files, on a Banana system
	struct Fw { long long id = 0, size = -1; std::string folder, name, md5, sha1; bool missing = false; };

	// DraStic's name for a DS file of its three, "" for any other
	inline std::string drasticName(const Fw& f)
	{
		if (f.folder != "nds") return "";
		std::string n = bsync::lower(f.name);
		return n == "bios7.bin" ? "nds_bios_arm7.bin" : n == "bios9.bin" ? "nds_bios_arm9.bin" : n == "firmware.bin" ? "nds_firmware.bin" : "";
	}
	// where a file goes, under /data: bios/<name>, and DraStic's copy
	inline std::vector<std::string> places(const Fw& f)
	{
		std::vector<std::string> out = { "bios/" + f.name };
		std::string d = drasticName(f);
		if (!d.empty()) out.push_back("drastic/system/" + d);
		return out;
	}
	// a file on the card against RomM's
	inline State stateOf(const std::string& path, const Fw& f)
	{
		if (!bsync::exists(path)) return MISSING;
		if (!f.md5.empty()) return bsync::hashFile(path, "md5") == f.md5 ? SAME : DIFFERENT;
		if (!f.sha1.empty()) return bsync::hashFile(path, "sha1") == f.sha1 ? SAME : DIFFERENT;
		return bsync::fileSize(path) == f.size ? SAME : DIFFERENT;
	}
	// the file's state, its places taken together: a different file kept anywhere first, then a place with none
	inline State stateOf(const Fw& f)
	{
		bool missing = false, different = false;
		for (auto& p : places(f))
		{
			State s = stateOf(bsync::ROOT + "/" + p, f);
			missing = missing || s == MISSING;
			different = different || s == DIFFERENT;
		}
		return different ? DIFFERENT : missing ? MISSING : SAME;
	}
	// a FirmwareSchema read; false for one of a platform Banana has no system for, or a name that can't be a file's
	inline bool read(const JVal& j, const std::map<long long, std::string>& mapped, Fw& f)
	{
		auto m = mapped.find((long long)j.num("platform_id"));
		if (m == mapped.end()) return false;
		f.id = (long long)j.num("id");
		f.folder = m->second;
		f.name = j.str("file_name");
		f.size = j.get("file_size_bytes") ? (long long)j.num("file_size_bytes", -1) : -1;
		f.md5 = bsync::lower(j.str("md5_hash"));
		f.sha1 = bsync::lower(j.str("sha1_hash"));
		f.missing = j.flag("missing_from_fs");
		return f.id > 0 && !f.name.empty() && f.name.find('/') == std::string::npos && bsync::safeRel(f.name);
	}
	// a firmware answer's why not: "scope" for RomM's 403 (the pairing without firmware.read)
	inline std::string why(const bsync::Resp& r, const std::string& what)
	{
		return r.code == 403 ? "scope" : r.code == 401 ? "unpaired" : what + ": " + r.error();
	}

	inline int list()
	{
		Paired p;
		std::string e = p.open();
		if (!e.empty()) return answer(e);
		std::map<long long, std::string> mapped;
		e = platformsMapped(*p.b, mapped);
		if (!e.empty()) return answer(e);
		std::vector<Fw> files;
		for (auto& pf : mapped)
		{
			bsync::Resp r = p.b->call("GET", "/api/firmware?platform_id=" + std::to_string(pf.first));
			if (r.code != 200) return answer(why(r, "firmware"));
			JPtr j = r.json();
			if (!j || !j->isArr()) return answer("the server's firmware can't be read");
			for (auto& x : j->arr)
			{
				Fw f;
				if (read(*x, mapped, f) && !f.missing) files.push_back(f);
			}
		}
		for (auto& f : files)
			printf("%lld\t%s\t%s\t%lld\t%s\n", f.id, f.folder.c_str(), f.name.c_str(), f.size, stateName(stateOf(f)));
		return answer("", "ok " + std::to_string(files.size()));
	}

	inline int get(const std::vector<std::string>& ids)
	{
		Paired p;
		std::string e = p.open();
		if (!e.empty()) return answer(e);
		std::map<long long, std::string> mapped;
		e = platformsMapped(*p.b, mapped);
		if (!e.empty()) return answer(e);
		// (every file asked of RomM first: a token that can't read them writes nothing)
		std::vector<Fw> files;
		for (auto& id : ids)
		{
			if (id.empty() || id.find_first_not_of("0123456789") != std::string::npos) return answer("not a firmware id: " + id);
			bsync::Resp r = p.b->call("GET", "/api/firmware/" + id);
			if (r.code == 404) return answer("no firmware " + id + " on the server");
			if (r.code != 200) return answer(why(r, "firmware " + id));
			JPtr j = r.json();
			Fw f;
			if (!j || !j->isObj() || !read(*j, mapped, f)) return answer("firmware " + id + " isn't for a system of Banana's");
			files.push_back(f);
		}
		int got = 0, same = 0, kept = 0;
		std::string failed;
		for (auto& f : files)
		{
			std::string url = p.c.server + "/api/firmware/" + std::to_string(f.id) + "/content/" + bsync::urlEsc(f.name);
			std::string algo = !f.md5.empty() ? "md5" : !f.sha1.empty() ? "sha1" : "", want = !f.md5.empty() ? f.md5 : f.sha1;
			std::string from;	// (a place holding RomM's file already, or downloaded to: the rest copied from it)
			for (auto& pl : places(f)) if (stateOf(bsync::ROOT + "/" + pl, f) == SAME) { from = bsync::ROOT + "/" + pl; break; }
			bool wrote = false, allSame = true, keptOne = false;
			for (auto& pl : places(f))
			{
				std::string path = bsync::ROOT + "/" + pl, err;
				State s = stateOf(path, f);
				if (s == SAME) { printf("%lld\t%s\tsame\n", f.id, pl.c_str()); continue; }
				allSame = false;
				if (s == DIFFERENT) { printf("%lld\t%s\tkept\n", f.id, pl.c_str()); keptOne = true; continue; }
				bool ok;
				if (!from.empty())
				{
					std::string data = bsync::readFile(from, &ok);
					ok = ok && bsync::writeAtomic(path, data, 0644) && stateOf(path, f) == SAME;
					if (!ok) { unlink(path.c_str()); err = "can't be written"; }
				}
				else
				{
					ok = bsync::fetchTo(url, p.b->auth(), path, f.size, algo, want, err, nullptr, nullptr, 16ll << 20);
					if (!ok) unlink((path + ".part").c_str());	// (nothing half there: a BIOS is small, fetched whole again)
					else from = path;
				}
				if (ok) { printf("%lld\t%s\tgot\n", f.id, pl.c_str()); wrote = true; }
				else
				{
					printf("%lld\t%s\tfailed %s\n", f.id, pl.c_str(), err.c_str());
					if (failed.empty()) failed = f.name + ": " + err;
				}
			}
			got += wrote;
			same += allSame;
			kept += keptOne;
		}
		if (!failed.empty()) return answer(failed);
		return answer("", "ok " + std::to_string(got) + " " + std::to_string(same) + " " + std::to_string(kept));
	}
}
