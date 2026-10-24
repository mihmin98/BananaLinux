#pragma once
// banana-ui (kiwami): rcheevos' disc reader, with .chd discs read through libchdr (third_party/libchdr, built by
// kiwami.mk) as RetroArch reads them (libretro-common's chd_stream.c and cdfs.c): a track's sector 0 is its INDEX 01
// (a pregap stored in the file is passed over), its sectors are numbered on from the frames of the tracks before it,
// and a raw sector's 2048 bytes of data are where the volume descriptor's "CD001" says (24 bytes in for CD-XA, as a
// PlayStation disc, else 16). Every other disc (.cue/.bin, .iso) goes to rcheevos' own reader. A .chd that can't be
// opened or read is reported to the iterator's error callback as "damaged .chd: <libchdr's reason>", before rcheevos
// says what it couldn't do without it.
struct rc_hash_cdreader;

namespace banana::kiwami::ra
{
	// fills reader (rc_hash_iterator_t's callbacks.cdreader) with these functions
	void chdReader(rc_hash_cdreader* reader);
}
