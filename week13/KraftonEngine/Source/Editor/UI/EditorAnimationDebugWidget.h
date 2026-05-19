#pragma once

#include "Editor/UI/EditorWidget.h"

class UAnimInstance;
struct FPropertyValue;

// 선택된 액터의 SkeletalMeshComponent → AnimInstance → FSM 라이브 상태를 표시하는
// read-only 디버그 패널. Property 패널 (편집) 과 보완적 — 여기는 진행 상황/타이밍 가시화.
//
// Visibility 는 FEditorSettings::UI.bAnimationDebug 가 단독 source of truth — MainPanel 에서 조건부 호출.
class FEditorAnimationDebugWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;

private:
	void RenderAnimGraphSection(UAnimInstance* AnimInst);   // RootNode 트리 시각화
	void RenderVariablesSection(UAnimInstance* AnimInst);
	void RenderRecentNotifiesSection(UAnimInstance* AnimInst);
	void RenderPropertyReadOnly(const FPropertyValue& P);
};
