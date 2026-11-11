// banana-ui (kiwami): the Game Boy Pokémon games' English character set (text.h). Generation II's, as pret's
// pokecrystal and pokegold define it (constants/charmap.asm: the games' own table); Gen I's differences come with Red,
// Blue and Yellow.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "text.h"

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
	}

	std::string decode(int gen, const uint8_t* p, size_t max)
	{
		(void)gen;
		std::string out;
		for (size_t i = 0; p && i < max && p[i] != 0x50; i++) out += p[i] >= 0x80 ? HIGH[p[i] - 0x80] : low(p[i]);
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
