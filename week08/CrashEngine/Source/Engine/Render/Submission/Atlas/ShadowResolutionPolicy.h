#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resources/Shadows/ShadowResolutionSettings.h"

// 요청 shadow 해상도를 atlas가 지원하는 2의 n승 tier로 정규화합니다.
EShadowResolution RoundShadowResolutionToTierPolicy(uint32 RequestedResolution);
