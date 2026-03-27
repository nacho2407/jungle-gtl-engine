#include "ShowFlags.h"



void FShowFlags::SetFlag(EEngineShowFlags InFlag, bool bEnabled)
{
	if (bEnabled)
		Flags |= static_cast<uint64>(InFlag);
	else
		Flags &= ~static_cast<uint64>(InFlag);


}

bool FShowFlags::HasFlag(EEngineShowFlags InFlag) const
{

	return (Flags & static_cast<uint64>(InFlag))!= 0;
}

void FShowFlags::ToggleFlag(EEngineShowFlags InFlag)
{
	Flags ^= static_cast<uint64>(InFlag); 
}
  