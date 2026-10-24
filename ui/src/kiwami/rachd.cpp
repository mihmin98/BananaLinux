// banana-ui (kiwami): rcheevos' disc reader with .chd through libchdr (rachd.h). A track handle is either one of
// rcheevos' own reader's (other) or a track of a .chd, read a hunk at a time.
#include "rachd.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <strings.h>
#include <vector>
// as kiwami.mk builds them: rcheevos' change rc_hash_iterator_t's layout, libchdr's which zlib codec_zlib.h names
#define RC_HASH_NO_ENCRYPTED
#define RC_HASH_NO_ZIP
#include "third_party/rcheevos/include/rc_hash.h"
#define CHDR_SYSTEM_ZLIB
#include "third_party/libchdr/include/libchdr/cdrom.h"
#include "third_party/libchdr/include/libchdr/chd.h"

namespace banana::kiwami::ra
{
	namespace
	{
		rc_hash_cdreader_t own;	// rcheevos' reader, for every disc but a .chd
		std::once_flag ownOnce;

		struct Track
		{
			void* other = nullptr;
			chd_file* chd = nullptr;
			const rc_hash_iterator_t* it = nullptr;	// whose error callback hears of a damaged .chd (once)
			bool told = false;
			uint32_t first = 0;		// the track's first sector on the disc
			uint32_t frame = 0;		// the CHD's frame holding that sector
			uint32_t sectors = 0;
			uint32_t at = 0;		// where a sector's 2048 bytes of data are in its frame
			uint32_t unit = 0, perHunk = 0;
			std::vector<uint8_t> hunk;
			int64_t loaded = -1;
		};

		// a track's line of CD metadata (CDROM_TRACK_METADATA2_FORMAT, or the older CDROM_TRACK_METADATA_FORMAT), its
		// strings bounded
		struct Meta { int track = 0, frames = 0, pregap = 0; char type[32] = "", pgtype[32] = ""; };

		chd_error meta(chd_file* chd, int i, Meta& m)
		{
			char s[256] = "", sub[32], pgsub[32];
			uint32_t n = 0;
			int postgap = 0;
			chd_error e = chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, i, s, sizeof s - 1, &n, nullptr, nullptr);
			if (e == CHDERR_NONE)
				return sscanf(s, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d",
					&m.track, m.type, sub, &m.frames, &m.pregap, m.pgtype, pgsub, &postgap) >= 4 ? e : CHDERR_INVALID_METADATA;
			if (e != CHDERR_METADATA_NOT_FOUND) return e;
			e = chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, i, s, sizeof s - 1, &n, nullptr, nullptr);
			if (e == CHDERR_NONE && sscanf(s, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d", &m.track, m.type, sub, &m.frames) != 4)
				e = CHDERR_INVALID_METADATA;
			return e;
		}

		void damaged(Track& t, chd_error e)
		{
			if (t.told || !t.it || !t.it->callbacks.error_message) return;
			t.told = true;
			t.it->callbacks.error_message(("damaged .chd: " + std::string(chd_error_string(e))).c_str(), t.it);
		}

		// sector s of the track's frame, or null when it can't be read
		const uint8_t* frameOf(Track& t, uint32_t s)
		{
			uint64_t f = (uint64_t)t.frame + s;
			int64_t h = (int64_t)(f / t.perHunk);
			if (h != t.loaded)
			{
				chd_error e = chd_read(t.chd, (uint32_t)h, t.hunk.data());
				if (e != CHDERR_NONE) { t.loaded = -1; damaged(t, e); return nullptr; }
				t.loaded = h;
			}
			return t.hunk.data() + (f % t.perHunk) * t.unit;
		}

		bool isChd(const char* path)
		{
			const char* dot = strrchr(path, '.');
			return dot && !strchr(dot, '/') && strcasecmp(dot, ".chd") == 0;
		}

		void* openTrack(const char* path, uint32_t track, const rc_hash_iterator_t* it)
		{
			std::call_once(ownOnce, [] { rc_hash_get_default_cdreader(&own); });
			Track* t = new Track;
			t->it = it;
			if (!isChd(path))
			{
				t->other = own.open_track_iterator(path, track, it);
				if (!t->other) { delete t; return nullptr; }
				return t;
			}
			chd_error e = chd_open(path, CHD_OPEN_READ, nullptr, &t->chd);
			if (e != CHDERR_NONE) { t->chd = nullptr; damaged(*t, e); delete t; return nullptr; }

			// The tracks in order, each one's frames padded to 4 in the file (CD_TRACK_PADDING) and its sectors counted
			// on the disc without; the one asked for as chd_stream.c finds it (rcheevos' special tracks included).
			Meta m, pick;
			bool found = false;
			uint32_t frame = 0, sector = 0, pickFrame = 0, pickSector = 0;
			int i = 0;
			for (; (e = meta(t->chd, i, m)) == CHDERR_NONE; i++)
			{
				bool data = strcmp(m.type, "AUDIO") != 0;
				if (m.frames < 0 || m.pregap < 0) { e = CHDERR_INVALID_METADATA; break; }
				bool want = track == RC_HASH_CDTRACK_FIRST_DATA ? data && !found
					: track == RC_HASH_CDTRACK_LAST ? true
					: track == RC_HASH_CDTRACK_LARGEST ? data && (!found || m.frames > pick.frames)
					: (uint32_t)m.track == track;
				if (want) { pick = m; pickFrame = frame; pickSector = sector; found = true; }
				sector += (uint32_t)m.frames;
				frame += ((uint32_t)m.frames + CD_TRACK_PADDING - 1) / CD_TRACK_PADDING * CD_TRACK_PADDING;
			}
			if (e != CHDERR_METADATA_NOT_FOUND || i == 0)
			{
				if (i == 0 && e == CHDERR_METADATA_NOT_FOUND) e = CHDERR_UNSUPPORTED_FORMAT;	// not a CD
				damaged(*t, e);
				chd_close(t->chd);
				delete t;
				return nullptr;
			}
			if (!found) { chd_close(t->chd); delete t; return nullptr; }	// rcheevos says which track it wanted

			const chd_header* hd = chd_get_header(t->chd);
			uint32_t pregap = pick.pgtype[0] == 'V' ? (uint32_t)pick.pregap : 0;	// stored before INDEX 01
			t->unit = hd->unitbytes;
			t->perHunk = t->unit ? hd->hunkbytes / t->unit : 0;
			t->first = pickSector;
			t->frame = pickFrame + pregap;
			t->sectors = pregap <= (uint32_t)pick.frames ? (uint32_t)pick.frames - pregap : 0;
			bool raw = !strcmp(pick.type, "MODE1_RAW") || !strcmp(pick.type, "MODE2_RAW");
			t->at = raw ? 16 : !strcmp(pick.type, "MODE2") || !strcmp(pick.type, "MODE2_FORM_MIX") ? 8 : 0;
			if (!t->perHunk || t->at + 2048 > t->unit)
			{
				damaged(*t, CHDERR_INVALID_DATA);
				chd_close(t->chd);
				delete t;
				return nullptr;
			}
			t->hunk.resize(hd->hunkbytes);
			// a raw sector's data: after a CD-XA subheader when the volume descriptor (sector 16) says "CD001" there
			if (raw && t->sectors > 16)
				if (const uint8_t* p = frameOf(*t, 16))
					if (!memcmp(p + 25, "CD001", 5)) t->at = 24;
			return t;
		}

		void* openTrackNoIterator(const char* path, uint32_t track) { return openTrack(path, track, nullptr); }

		size_t readSector(void* handle, uint32_t sector, void* buffer, size_t bytes)
		{
			Track& t = *(Track*)handle;
			if (t.other) return own.read_sector(t.other, sector, buffer, bytes);
			if (sector < t.first) return 0;
			// the data of as many sectors as it takes, from this one on
			uint8_t* out = (uint8_t*)buffer;
			size_t done = 0;
			for (uint32_t s = sector - t.first; done < bytes && s < t.sectors; s++)
			{
				const uint8_t* p = frameOf(t, s);
				if (!p) break;
				size_t n = bytes - done < 2048 ? bytes - done : 2048;
				memcpy(out + done, p + t.at, n);
				done += n;
			}
			return done;
		}

		void closeTrack(void* handle)
		{
			Track* t = (Track*)handle;
			if (t->other) own.close_track(t->other);
			if (t->chd) chd_close(t->chd);
			delete t;
		}

		uint32_t firstSector(void* handle)
		{
			Track& t = *(Track*)handle;
			return t.other ? own.first_track_sector(t.other) : t.first;
		}
	}

	void chdReader(rc_hash_cdreader* reader)
	{
		reader->open_track = openTrackNoIterator;
		reader->open_track_iterator = openTrack;
		reader->read_sector = readSector;
		reader->close_track = closeTrack;
		reader->first_track_sector = firstSector;
	}
}
