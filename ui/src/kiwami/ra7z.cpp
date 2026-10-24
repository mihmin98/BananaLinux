// banana-ui (kiwami): one file of a .7z (ra7z.h), through the LZMA SDK's SzArEx_Open and SzArEx_Extract over a FILE*.
#include "ra7z.h"
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include "third_party/lzma/7z.h"
#include "third_party/lzma/7zAlloc.h"
#include "third_party/lzma/7zCrc.h"

namespace banana::kiwami::ra
{
	namespace
	{
		// the archive as the SDK reads it: vt first, so the SDK's pointer to it is one to the whole
		struct In { ISeekInStream vt; FILE* f; };

		SRes inRead(ISeekInStreamPtr p, void* buf, size_t* size)
		{
			FILE* f = ((const In*)(const void*)p)->f;
			size_t n = *size ? fread(buf, 1, *size, f) : 0;
			*size = n;
			return n == 0 && ferror(f) ? SZ_ERROR_READ : SZ_OK;
		}
		SRes inSeek(ISeekInStreamPtr p, Int64* pos, ESzSeek origin)
		{
			FILE* f = ((const In*)(const void*)p)->f;
			int whence = origin == SZ_SEEK_SET ? SEEK_SET : origin == SZ_SEEK_CUR ? SEEK_CUR : SEEK_END;
			if (fseeko(f, (off_t)*pos, whence) != 0) return SZ_ERROR_READ;
			*pos = (Int64)ftello(f);
			return SZ_OK;
		}

		// a file's name in the archive, UTF-16 there, in UTF-8
		std::string utf8(const std::vector<UInt16>& w)
		{
			std::string s;
			for (size_t i = 0; i < w.size() && w[i]; i++)
			{
				uint32_t c = w[i];
				if (c >= 0xD800 && c < 0xDC00 && i + 1 < w.size() && w[i + 1] >= 0xDC00 && w[i + 1] < 0xE000)
					c = 0x10000 + ((c - 0xD800) << 10) + (w[++i] - 0xDC00);
				if (c < 0x80) s += (char)c;
				else if (c < 0x800) { s += (char)(0xC0 | c >> 6); s += (char)(0x80 | (c & 0x3F)); }
				else if (c < 0x10000) { s += (char)(0xE0 | c >> 12); s += (char)(0x80 | (c >> 6 & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
				else
				{
					s += (char)(0xF0 | c >> 18); s += (char)(0x80 | (c >> 12 & 0x3F));
					s += (char)(0x80 | (c >> 6 & 0x3F)); s += (char)(0x80 | (c & 0x3F));
				}
			}
			return s;
		}
	}

	Unpack read7z(const std::string& path, const std::function<int(const std::vector<rnds::ZipMember>&)>& pick,
		uint64_t max, const std::function<void(const uint8_t*, size_t)>& use)
	{
		static std::once_flag crc;
		std::call_once(crc, CrcGenerateTable);
		In in;
		in.vt.Read = inRead;
		in.vt.Seek = inSeek;
		if (!(in.f = fopen(path.c_str(), "rb"))) return Unpack::DAMAGED;
		CLookToRead2 look;
		LookToRead2_CreateVTable(&look, False);
		look.realStream = &in.vt;
		look.bufSize = 1 << 16;
		look.buf = (Byte*)malloc(look.bufSize);
		LookToRead2_INIT(&look);
		const ISzAlloc alloc = { SzAlloc, SzFree }, temp = { SzAllocTemp, SzFreeTemp };
		CSzArEx db;
		SzArEx_Init(&db);
		Unpack r = Unpack::DAMAGED;
		if (look.buf && SzArEx_Open(&db, &look.vt, &alloc, &temp) == SZ_OK)
		{
			std::vector<rnds::ZipMember> files;
			std::vector<UInt32> index;
			std::vector<UInt16> name;
			for (UInt32 i = 0; i < db.NumFiles; i++)
			{
				if (SzArEx_IsDir(&db, i)) continue;
				name.assign(SzArEx_GetFileNameUtf16(&db, i, nullptr), 0);
				SzArEx_GetFileNameUtf16(&db, i, name.data());
				rnds::ZipMember m;
				m.name = utf8(name);
				m.size = SzArEx_GetFileSize(&db, i);
				files.push_back(m);
				index.push_back(i);
			}
			int k = pick(files);
			if (k < 0 || k >= (int)files.size()) r = Unpack::NONE;
			else
			{
				UInt32 i = index[k], folder = db.FileToFolder[i];
				uint64_t block = folder == (UInt32)-1 ? 0 : SzAr_GetFolderUnpackSize(&db.db, folder);	// -1: an empty file
				if (files[k].size > max || block > max) r = Unpack::TOO_BIG;
				else
				{
					UInt32 cached = (UInt32)-1;
					Byte* out = nullptr;
					size_t outSize = 0, at = 0, size = 0;
					if (SzArEx_Extract(&db, &look.vt, i, &cached, &out, &outSize, &at, &size, &alloc, &temp) == SZ_OK)
					{
						use(out ? out + at : (const uint8_t*)"", size);
						r = Unpack::OK;
					}
					ISzAlloc_Free(&alloc, out);
				}
			}
		}
		SzArEx_Free(&db, &alloc);
		free(look.buf);
		fclose(in.f);
		return r;
	}
}
