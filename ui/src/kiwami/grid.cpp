// banana-ui (kiwami): Settings' grid of groups with a last row that isn't full (grid.h).
#include "grid.h"
#include <algorithm>

namespace banana
{
	namespace kiwami
	{
		int gridMove(int g, int n, bool up)
		{
			int last = (n - 1) / 3;
			if (!up)
			{
				if (g + 3 < n) return g + 3;
				return g / 3 < last ? n - 1 : g % 3;	// (from the row above the last: its last tile)
			}
			if (g - 3 >= 0) return g - 3;
			return std::min(n - 1, last * 3 + g % 3);
		}

		int gridTop(int row0, int gsel, int n)
		{
			int rows = (n + 2) / 3, r = gsel / 3;
			row0 = std::max(r - 1, std::min(row0, r));
			return std::max(0, std::min(row0, std::max(0, rows - 2)));
		}
	}
}
