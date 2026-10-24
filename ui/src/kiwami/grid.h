#pragma once
// banana-ui (kiwami): Settings' grid of groups with a last row that isn't full (the RetroAchievements group, a seventh
// tile): three tiles a row, two rows shown at a time. Plain arithmetic, so the unit test checks it without a screen.
namespace banana
{
	namespace kiwami
	{
		// UP or DOWN from group g of n: the tile above or below; past the last row, round to the first row's tile in the
		// same column; above the first, the last row's in the same column, or its last tile when that column is empty.
		// With n a multiple of 3 it is upstream's (g ± 3) % n
		int gridMove(int g, int n, bool up);
		// the first of the two rows shown, from the one shown so far: it moves only as far as needed to show gsel's row
		int gridTop(int row0, int gsel, int n);
	}
}
