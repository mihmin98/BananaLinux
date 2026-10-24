# banana-ui (kiwami): the fork's C libraries, vendored unchanged in src/kiwami/third_party/ (each folder's SHA256SUMS
# lists its files): rcheevos' hashing, for the RetroAchievements pages' game identification (rahash.cpp). Included by
# the Makefile (its third kiwami: line), before banana-ui's rule reads SRC.
# Each file is compiled to <file>.kiwami.o with the C++ compiler's driver as C: Buildroot passes only CXX to banana-ui's
# build (buildroot/package/banana-ui/banana-ui.mk), and builds with make -B, so objects left by a host build in its
# copy are built again for the target, never linked. The objects go into SRC and are linked by the Makefile's own rule.
#   make kiwami-c      the objects alone (kiwami/tests/ci.sh links its unit test with them)
KIWAMI_TP = src/kiwami/third_party
KIWAMI_C = $(addprefix $(KIWAMI_TP)/rcheevos/src/,rc_compat.c rc_util.c rhash/cdreader.c rhash/hash.c rhash/hash_disc.c \
	rhash/hash_rom.c rhash/md5.c)
KIWAMI_O = $(KIWAMI_C:.c=.kiwami.o)
# the target's flags without the C++ standard and the warnings (vendored code's own aren't the fork's to fix);
# RC_HASH_NO_ENCRYPTED and RC_HASH_NO_ZIP as rahash.cpp defines them
KIWAMI_CFLAGS = $(filter-out -std=% -W%,$(CXXFLAGS)) -std=gnu11 -w -I$(KIWAMI_TP)/rcheevos/include \
	-DRC_HASH_NO_ENCRYPTED -DRC_HASH_NO_ZIP

%.kiwami.o: %.c $(wildcard $(KIWAMI_TP)/*/*.h $(KIWAMI_TP)/*/*/*.h $(KIWAMI_TP)/*/*/*/*.h) src/kiwami/kiwami.mk
	$(CXX) -x c $(KIWAMI_CFLAGS) -c -o $@ $<

SRC += $(KIWAMI_O)

kiwami-c: $(KIWAMI_O)
kiwami-clean:
	rm -f $(KIWAMI_O)
clean: kiwami-clean
.PHONY: kiwami-c kiwami-clean
