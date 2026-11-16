// banana-ui (kiwami): the Game Boy Pokémon games' English character set (text.h). Generation II's, as pret's
// pokecrystal and pokegold define it (constants/charmap.asm: the games' own table), where Generation I's differs, as
// pokered and pokeyellow define it, and where PureRGB's differs from that, as its constants/charmap.asm (v2.7.7)
// defines it (PokeDaisy's Gen1Charset.PURE has the same).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "text.h"
#include <algorithm>
#include <cstdio>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// 0x80 up: letters, punctuation, the contractions ('d 'l 'm 'r 's 't 'v: one tile each), the PK and MN
		// ligatures, the arrows and the cursors, the gender signs, and the digits; "" where there's none
		const char* const HIGH[128] = {
			"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",				// 0x80
			"Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "(", ")", ":", ";", "[", "]",				// 0x90
			"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p",				// 0xA0
			"q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "", "", "", "", "", "",					// 0xB0
			"Ä", "Ö", "Ü", "ä", "ö", "ü", "", "", "", "", "", "", "", "", "", "",						// 0xC0
			"'d", "'l", "'m", "'r", "'s", "'t", "'v", "", "", "", "", "", "", "", "", "←",				// 0xD0
			"'", "PK", "MN", "-", "", "", "?", "!", ".", "&", "é", "→", "▷", "▶", "▼", "♂",				// 0xE0
			"¥", "×", ".", "/", ",", "♀", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",				// 0xF0
		};
		// below 0x80, the few a name or a ROM's text holds: "#" (0x54) is POKé, the quotation marks, the dots, the
		// space; the line and page breaks of a description or a Pokédex page (<NEXT>, <LINE>, <PARA>, <CONT>) and the
		// breakable spaces of a place's name (<BSP>, <LF>) as a space; the rest are the text's commands or Japanese
		const char* low(uint8_t b)
		{
			switch (b)
			{
			case 0x1F: case 0x22: case 0x4E: case 0x4F: case 0x51: case 0x55: return " ";
			case 0x4A: return "PKMN";
			case 0x54: return "POKé";
			case 0x56: return "……";
			case 0x72: return "“";
			case 0x73: return "”";
			case 0x74: return "·";
			case 0x75: return "…";
			case 0x7F: return " ";
			default: return "";
			}
		}
		// Gen I's own: é and the contractions 'd 'l 's 't 'v at BA-BF, 'r and 'm at E4-E5, the single quotes at 70-71,
		// a Pokédex page's break (<PAGE>, 49) as a space; no umlauts, no ← & → and no breakable spaces; nullptr where
		// it's Gen II's
		const char* gen1(uint8_t b)
		{
			static const char* const BA[6] = { "é", "'d", "'l", "'s", "'t", "'v" };
			if (b >= 0xBA && b <= 0xBF) return BA[b - 0xBA];
			if ((b >= 0xC0 && b <= 0xC5) || (b >= 0xD0 && b <= 0xD6) || b == 0xDF || (b >= 0xE9 && b <= 0xEB)) return "";
			if (b == 0xE4) return "'r";
			if (b == 0xE5) return "'m";
			if (b == 0x70) return "‘";
			if (b == 0x71) return "’";
			if (b == 0x1F || b == 0x22) return "";
			if (b == 0x49) return " ";
			return nullptr;
		}
		// PureRGB's own over Gen I's: one-byte words at 33-47 and 4D (home/text.asm expands them: "<opponent>" and
		// "<user>" are the words), a page break that waits on more buttons (<BAGE>, 48) as a space, its ellipsis of three
		// dots, PC, TM, TRAINER and ROCKET at 5B-5E, the middle dot, curly quotes where the brackets were, an arrow, a
		// plus and a percent sign; nullptr where it's Gen I's
		const char* pure(uint8_t b)
		{
			static const char* const WORDS[21] = { "like", "an", "You", "er", "with", "at", "ight", "have", " a ", "here",
				"to ", " to", "or", "ing", "the", "you", "opponent", "user", "POKéMON", "TRAINER TIPS", "TEAM " };
			if (b >= 0x33 && b <= 0x47) return WORDS[b - 0x33];
			switch (b)
			{
			case 0x48: return " ";
			case 0x4D: return "is";
			case 0x56: return "...";
			case 0x5B: return "PC";
			case 0x5C: return "TM";
			case 0x5D: return "TRAINER";
			case 0x5E: return "ROCKET";
			case 0x74: return "·";
			case 0x9E: return "“";
			case 0x9F: return "”";
			case 0xE9: return "→";
			case 0xEA: return "+";
			case 0xEB: return "%";
			default: return nullptr;
			}
		}
	}

	std::string decode(Charset cs, const uint8_t* p, size_t max)
	{
		std::string out;
		for (size_t i = 0; p && i < max && p[i] != 0x50; i++)
		{
			const char* own = cs == Charset::PURE ? pure(p[i]) : nullptr;
			const char* g1 = !own && cs != Charset::GEN2 ? gen1(p[i]) : nullptr;
			out += own ? own : g1 ? g1 : p[i] >= 0x80 ? HIGH[p[i] - 0x80] : low(p[i]);
		}
		return out;
	}

	namespace
	{
		const int DICT_END = 0x60;			// (PlaceNextChar's cp $60: from it the bytes are characters)
		const int DICT_DEPTH = 4, WORD_MAX = 64;
		const uint8_t BSP = 0x1F, TERMINATOR = 0x50;
		enum { DICT_WORD = 0, DICT_ROUTINE = 1 };
		// the routines a byte of Crystal's control characters stands for that decode as the character set has them:
		// the line and page breaks and the word break (pokecrystal's constants/charmap.asm)
		bool knownRoutine(uint8_t b) { return b == 0x22 || b == 0x25 || b == 0x4E || b == 0x4F || b == 0x51 || b == 0x55; }

		// p's bytes up to max or the terminator, the dictionary's words expanded (depth: how deep in words); false
		// when an unknown routine stopped it
		bool packed(Charset cs, const uint8_t* p, size_t max, const TextDict& dict, int depth, std::string& out)
		{
			for (size_t i = 0; p && i < max && p[i] != TERMINATOR; i++)
			{
				const uint8_t c = p[i];
				const size_t e = c >= dict.first ? dict.table + 3 * (size_t)(c - dict.first) : dict.size;
				if (c >= DICT_END || c == BSP || e + 3 > dict.size)
				{
					out += decode(cs, &p[i], 1);
					continue;
				}
				const uint8_t kind = dict.rom[e];
				const uint16_t to = (uint16_t)(dict.rom[e + 1] | dict.rom[e + 2] << 8);
				if (kind == DICT_WORD && to < 0x4000 && to < dict.size && depth < DICT_DEPTH)
				{
					if (!packed(cs, dict.rom + to, std::min<size_t>(WORD_MAX, dict.size - to), dict, depth + 1, out)) return false;
				}
				else if ((kind == DICT_WORD && to >= 0x4000) || (kind == DICT_ROUTINE && knownRoutine(c)))
					out += decode(cs, &p[i], 1);
				else
				{
					static bool logged = false;
					if (!logged)
						fprintf(stderr, "banana-ui: gb companion: text: unknown routine for 0x%02X, the text stops there\n", c);
					logged = true;
					return false;
				}
			}
			return true;
		}
	}

	std::string decode(Charset cs, const uint8_t* p, size_t max, const TextDict& dict)
	{
		std::string out;
		if (!dict.rom || dict.first < 1 || dict.first >= DICT_END) return decode(cs, p, max);
		packed(cs, p, max, dict, 0, out);
		return out;
	}

	std::string nameCase(const std::string& s)
	{
		// each word's first letter kept and the rest lower case; a word starts after a space, '-' or '.'. HP, PP, PKMN,
		// GS, PC, TV and a TM or HM with its number stay as they are (upstream's gba::nameCase keeps the same)
		static const char* const KEEP[] = { "HP", "PP", "PKMN", "GS", "PC", "TV" };
		std::string o = s;
		size_t i = 0;
		while (i < o.size())
		{
			size_t j = i;
			while (j < o.size() && o[j] != ' ' && o[j] != '-' && o[j] != '.') j++;
			const std::string w = o.substr(i, j - i);
			bool keep = (w.size() > 2 && (w.compare(0, 2, "TM") == 0 || w.compare(0, 2, "HM") == 0) && w[2] >= '0' && w[2] <= '9');
			for (const char* k : KEEP) keep = keep || w == k;
			if (!keep)
				for (size_t k = i + 1; k < j; k++)
					if (o[k] >= 'A' && o[k] <= 'Z') o[k] = (char)(o[k] - 'A' + 'a');
			i = j + 1;
		}
		return o;
	}
}
