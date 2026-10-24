# banana-ui (kiwami): the fork's C libraries, vendored unchanged in src/kiwami/third_party/ (each folder's SHA256SUMS
# lists its files): rcheevos' hashing, for the RetroAchievements pages' game identification (rahash.cpp), the LZMA
# SDK's 7z decoder, for the games in a .7z (ra7z.cpp), and libchdr, for PlayStation .chd discs (rachd.cpp). Included by
# the Makefile (its third kiwami: line), before banana-ui's rule reads SRC.
# Each file is compiled to <file>.kiwami.o with the C++ compiler's driver as C: Buildroot passes only CXX to banana-ui's
# build (buildroot/package/banana-ui/banana-ui.mk), and builds with make -B, so objects left by a host build in its
# copy are built again for the target, never linked. The objects go into SRC and are linked by the Makefile's own rule.
#   make kiwami-c      the objects alone (kiwami/tests/ci.sh links its unit test with them)
KIWAMI_TP = src/kiwami/third_party
KIWAMI_C = $(addprefix $(KIWAMI_TP)/rcheevos/src/,rc_compat.c rc_util.c rhash/cdreader.c rhash/hash.c rhash/hash_disc.c \
	rhash/hash_rom.c rhash/md5.c)
KIWAMI_C += $(addprefix $(KIWAMI_TP)/lzma/,7zAlloc.c 7zArcIn.c 7zBuf.c 7zCrc.c 7zCrcOpt.c 7zDec.c 7zStream.c Bcj2.c Bra.c \
	Bra86.c BraIA64.c CpuArch.c Delta.c Lzma2Dec.c LzmaDec.c)
# libchdr with its own LZMA (its symbols renamed CHDR_*, so beside lzma/'s) and zstd's decompressor; zlib is the
# system's, which banana-ui links already
KIWAMI_C += $(addprefix $(KIWAMI_TP)/libchdr/,$(addprefix src/libchdr_,bitstream.c cdrom.c chd.c codec_avhuff.c codec_cdfl.c \
	codec_cdlz.c codec_cdzl.c codec_cdzs.c codec_flac.c codec_huff.c codec_lzma.c codec_zlib.c codec_zstd.c flac.c \
	huffman.c) deps/lzma-26.02/src/LzmaDec.c deps/zstd-1.5.7/zstddeclib.c)
KIWAMI_O = $(KIWAMI_C:.c=.kiwami.o)
# the target's flags without the C++ standard and the warnings (vendored code's own aren't the fork's to fix);
# RC_HASH_NO_ENCRYPTED and RC_HASH_NO_ZIP as rahash.cpp defines them, CHDR_SYSTEM_ZLIB as rachd.cpp does
KIWAMI_CFLAGS = $(filter-out -std=% -W%,$(CXXFLAGS)) -std=gnu11 -w -I$(KIWAMI_TP)/rcheevos/include \
	-DRC_HASH_NO_ENCRYPTED -DRC_HASH_NO_ZIP -DCHDR_SYSTEM_ZLIB

%.kiwami.o: %.c $(wildcard $(KIWAMI_TP)/*/*.h $(KIWAMI_TP)/*/*/*.h $(KIWAMI_TP)/*/*/*/*.h) src/kiwami/kiwami.mk
	$(CXX) -x c $(KIWAMI_CFLAGS) -c -o $@ $<

SRC += $(KIWAMI_O)

kiwami-c: $(KIWAMI_O)
kiwami-clean:
	rm -f $(KIWAMI_O)
clean: kiwami-clean
.PHONY: kiwami-c kiwami-clean
