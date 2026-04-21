#pragma once

#include "Render/Submission/Batching/LineBatch.h"

/*
    FOverlayBatchSet은 Renderer가 프레임 중 임시로 사용하는 overlay line batch 묶음입니다.
    월드 텍스트는 Primitive 경로로 처리하므로 여기에는 grid/debug line만 둡니다.
*/
struct FOverlayBatchSet
{
    FLineBatch GridLines;
    FLineBatch DebugLines;
};
