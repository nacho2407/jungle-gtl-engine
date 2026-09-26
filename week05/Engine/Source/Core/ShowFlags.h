#pragma once
#include "CoreMinimal.h"
#include "EngineAPI.h"
enum class EEngineShowFlags : uint64
{
	SF_Primitives = 1 << 0,
	SF_UUID = 1 << 1,
	SF_DebugDraw = 1 <<2,
	SF_WorldAxis = 1 <<3,
	SF_Collision =1<<4,
	SF_Billboard = 1 << 5,
	SF_Text = 1 << 6,
	SF_Grid = 1<<7,
	 // SF_Grid        = 1 << 3,
	 // SF_Fog         = 1 << 4,
};
class ENGINE_API FShowFlags
{
public:
	FShowFlags()
		: Flags(
			static_cast<uint64>(EEngineShowFlags::SF_Primitives) |
			static_cast<uint64>(EEngineShowFlags::SF_UUID) |
			static_cast<uint64>(EEngineShowFlags::SF_Billboard) |
			static_cast<uint64>(EEngineShowFlags::SF_Text)) {
	}
	void SetFlag(EEngineShowFlags InFlag, bool bEnabled);
	bool HasFlag(EEngineShowFlags InFlag)const;
	void ToggleFlag(EEngineShowFlags InFlag);
private:
	uint64 Flags;
};