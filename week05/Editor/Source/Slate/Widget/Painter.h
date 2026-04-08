#pragma once

#include "Viewport/ViewportTypes.h"
#include "Renderer/UIDrawList.h"

#ifdef DrawText
#undef DrawText
#endif

class FSlatePaintContext
{
public:
	FSlatePaintContext() = default;

	// 지금 기록 중인 드로우 리스트의 대상 화면 크기를 저장한다.
	void SetScreenSize(int32 Width, int32 Height);

	// 채워진 사각형 드로우 엘리먼트를 기록한다.
	void DrawRectFilled(FRect Rect, uint32 Color);
	// 사각형 외곽선 드로우 엘리먼트를 기록한다.
	void DrawRect(FRect Rect, uint32 Color);
	// 텍스트 드로우 엘리먼트를 기록한다.
	void DrawText(FPoint Point, const char* Text, uint32 Color, float FontSize, float LetterSpacing);
	// 현재 텍스트 규칙으로 문자열의 화면 크기를 대략 계산한다.
	FVector2 MeasureText(const char* Text, float FontSize, float LetterSpacing);
	// 이후 엘리먼트에 적용할 클리핑 사각형을 푸시한다.
	void PushClipRect(const FRect& InRect);
	// 가장 최근에 푸시한 클리핑 사각형을 팝한다.
	void PopClipRect();
	// 이후 엘리먼트에 적용할 UI 상대 깊이 오프셋을 푸시한다.
	void PushDepth(float InDepth);
	// 가장 최근에 푸시한 UI 깊이값을 팝한다.
	void PopDepth();
	// 이후 엘리먼트에 적용할 UI 상대 레이어 오프셋을 푸시한다.
	void PushLayer(int32 InLayer);
	// 가장 최근에 푸시한 UI 레이어값을 팝한다.
	void PopLayer();

	// 현재까지 기록된 드로우 리스트를 읽기 전용으로 반환한다.
	const FUIDrawList& GetDrawList() const { return DrawList; }
	// 기록된 드로우 리스트를 꺼내고 기록기를 초기 상태로 되돌린다.
	FUIDrawList ConsumeDrawList();
	// 현재 드로우 리스트, 클립 스택, 순서 상태를 모두 비운다.
	void Reset();

private:
	// 준비된 드로우 엘리먼트를 리스트 뒤에 추가한다.
	void AppendElement(FUIDrawElement&& Element);
	// 현재 활성 클립을 입력 사각형에 적용한다.
	FRect ApplyCurrentClip(const FRect& InRect) const;
	// 현재 유효한 클리핑 사각형이 활성화되어 있으면 true를 반환한다.
	bool HasActiveClip() const;

private:
	FUIDrawList DrawList;
	uint64 NextOrder = 0;
	TArray<FRect> ClipStack;
	TArray<int32> LayerStack;
	int32 CurrentLayer = 0;
	TArray<float> DepthStack;
	float CurrentDepth = 0.0f;
};

using FPainter = FSlatePaintContext;
