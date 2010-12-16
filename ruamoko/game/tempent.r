#include "qw_message.h"
#include "tempent.h"

void(vector org, float damage) SpawnBlood =
{
	WriteBytes (MSG_MULTICAST, (float) SVC_TEMPENTITY, (float) TE_BLOOD, 1.0);
	WriteCoordV (MSG_MULTICAST, org);
	multicast (org, MULTICAST_PVS);
};
