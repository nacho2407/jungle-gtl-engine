#pragma once

#include "Render/Submission/Atlas/ShadowAtlasTypes.h"

// Shadow atlas rect에서 실제 렌더링 viewport와 shader UV 복원에 쓰는
// 보조 metadata를 계산하는 공통 함수들입니다.
FShadowAtlasRect MakeShadowAtlasViewportRect(const FShadowAtlasRect& Rect);
FVector4         MakeShadowAtlasUVScaleOffset(const FShadowAtlasRect& ViewportRect);
