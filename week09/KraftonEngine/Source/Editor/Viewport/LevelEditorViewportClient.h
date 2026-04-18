#pragma once

#include "Editor/Viewport/EditorViewportClient.h"

// UE의 FLevelEditorViewportClient 대응
// 레벨 편집 전용 뷰포트 (카메라 조작, 기즈모, 액터 피킹 등)
// 현재는 FEditorViewportClient의 기존 기능을 그대로 사용
class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient() = default;
	~FLevelEditorViewportClient() override = default;
};
