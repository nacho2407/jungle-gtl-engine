#pragma once

#include "Core/CoreTypes.h"

struct FOcclusionDebugState
{
	bool   bEnabled = false;
	bool   bEligible = false;
	bool   bUsingCachedResults = false;
	bool   bSubmittedThisFrame = false;
	bool   bReadbackPending = false;
	bool   bHasCachedResults = false;
	bool   bHasReadbackResult = false;
	bool   bAcceptedCullResult = false;
	uint32 VisibleCount = 0;
	uint32 ProposedSurvivorCount = 0;
	uint32 CachedSurvivorCount = 0;
	uint32 CullSavings = 0;
	uint32 RequiredSavings = 0;
	uint32 LastSubmittedCount = 0;
};
