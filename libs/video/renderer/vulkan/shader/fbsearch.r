// Find the largest index of the array with a value <= key.
// Entries in the array must be sorted (returns last of duplicates)
uint fbsearch (const uint key, uint *array, const uint count)
{
	uint left = 0;
	uint right = count - 1;
	uint mid;

	if (!count) {
		return ~0;
	}
	while (left != right) {
		mid = (left + right + 1) / 2;
		if (key < array[mid]) {
			right = mid - 1;
		} else {
			left = mid;
		}
	}
	return (key >= array[left]) ? left : ~0;
}
