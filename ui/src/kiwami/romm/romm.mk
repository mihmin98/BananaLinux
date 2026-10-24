# The RomM helper (kiwami), /usr/libexec/kiwami/romm: its own program, not part of banana-ui (the wrapper around the
# games runs it outside the menu), so ui/Makefile doesn't build it. Run in ui/:
#   make -f src/kiwami/romm/romm.mk OUT=<dir>       <dir>/romm (the package kiwami-romm, and kiwami/tests/romm-helper.sh
#                                                   for the host)
# rcheevos' md5.c (vendored for RetroAchievements) gives the saves' hashes, compiled as C as kiwami.mk does.
OUT ?= .
CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -O2
CFLAGS ?= -O2
ROMM_SRC = $(wildcard src/kiwami/romm/*.cpp) src/kiwami/json.cpp src/kiwami/conf.cpp src/kiwami/rommcard.cpp
ROMM_MD5 = src/kiwami/third_party/rcheevos/src/rhash/md5.c

$(OUT)/romm: $(ROMM_SRC) $(wildcard src/kiwami/romm/*.h) src/kiwami/json.h src/kiwami/conf.h src/kiwami/rommcard.h $(ROMM_MD5) \
		src/kiwami/romm/romm.mk
	$(CC) -x c $(CFLAGS) -std=gnu11 -w -c -o $(OUT)/romm-md5.o $(ROMM_MD5)
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -I src -o $@ $(ROMM_SRC) $(OUT)/romm-md5.o $(LDFLAGS) -ldl
