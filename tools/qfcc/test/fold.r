float FL_MONSTER = 32;
float FL_FLY = 1;
float FL_SWIM = 2;
.integer flags;
void (entity other)
trigger_monsterjump_touch =
{
	if (other.flags & (FL_MONSTER | FL_FLY | FL_SWIM) != FL_MONSTER)
		return;
};
