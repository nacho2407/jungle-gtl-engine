#pragma once

// Shadow atlas 관련 타입과 구현을 한 번에 가져오기 위한 umbrella 헤더입니다.
// 외부 코드는 가급적 이 헤더만 include하고, 내부 구현은 세부 헤더로 분리해 둡니다.
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"
#include "Render/Submission/Atlas/AtlasPageBase.h"
#include "Render/Submission/Atlas/AtlasBuddyAllocator2D.h"
#include "Render/Submission/Atlas/ShadowAtlasPage.h"
#include "Render/Submission/Atlas/ShadowAtlasRegistry.h"
