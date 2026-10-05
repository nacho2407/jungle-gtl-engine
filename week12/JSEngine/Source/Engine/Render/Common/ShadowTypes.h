#pragma once
#include "Core/CoreTypes.h"
#include "Core/Reflection/ReflectionMacros.h"

// ShadowMap Method
UENUM()
enum class EShadowMap : uint32
{
	CSM UMETA(DisplayName = "CSM"),
	PSM UMETA(DisplayName = "PSM"),
	MAX UMETA(Hidden)
};

