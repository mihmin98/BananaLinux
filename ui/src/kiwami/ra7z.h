#pragma once
// banana-ui (kiwami): one file of a .7z, unpacked into memory, for rahash (a .7z cartridge is hashed on the game in it,
// as RetroArch hashes it). The LZMA SDK's 7z decoder (third_party/lzma, built by kiwami.mk): LZMA, LZMA2, and the
// branch (BCJ, BCJ2, ARM, ARM64, RISC-V...) and Delta filters; no PPMd, no encryption.
#include "../../rnds/RndsRaster.h"

namespace banana::kiwami::ra
{
	enum class Unpack { OK, NONE, TOO_BIG, DAMAGED };	// NONE: pick chose none

	// The file pick chooses from the archive's files (their paths in it, in UTF-8, and unpacked sizes; -1 for none),
	// handed to use whole, when unpacking it takes max bytes at most: a solid archive unpacks the whole block the file
	// is in, and that is checked before anything is allocated. Checked against the archive's CRC.
	Unpack read7z(const std::string& path, const std::function<int(const std::vector<rnds::ZipMember>&)>& pick,
		uint64_t max, const std::function<void(const uint8_t*, size_t)>& use);
}
