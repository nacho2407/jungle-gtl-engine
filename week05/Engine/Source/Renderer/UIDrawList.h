#pragma once

#include "CoreMinimal.h"

struct ENGINE_API FUIRect
{
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;

	// 실제로 그릴 수 있는 크기면 true를 반환한다.
	bool IsValid() const { return Width > 0.0f && Height > 0.0f; }
};

struct ENGINE_API FUIPoint
{
	float X = 0.0f;
	float Y = 0.0f;
};

enum class EUIDrawElementType : uint8
{
	FilledRect,
	RectOutline,
	Text,
};

struct ENGINE_API FUIDrawElement
{
	// 이 엘리먼트가 어떤 화면 공간 프리미티브인지 나타낸다.
	EUIDrawElementType Type = EUIDrawElementType::FilledRect;
	FUIRect Rect = {};
	FUIPoint Point = {};
	uint32 Color = 0xFFFFFFFF;
	FString Text;
	float FontSize = 12.0f;
	float LetterSpacing = 1.0f;
	int32 Layer = 0;
	// 같은 Layer 내부에서 화면 앞/뒤 순서를 결정하는 키다.
	float Depth = 0.0f;
	uint64 Order = 0;
	bool bHasClipRect = false;
	FUIRect ClipRect = {};
};

struct ENGINE_API FUIDrawList
{
	// 현재 프레임에 Slate가 기록한 드로우 엘리먼트 목록이다.
	TArray<FUIDrawElement> Elements;
	// 드로우 리스트를 만들 때 사용한 화면 크기다.
	int32 ScreenWidth = 0;
	int32 ScreenHeight = 0;

	// 드로우 리스트와 화면 메트릭을 모두 비운다.
	void Clear()
	{
		Elements.clear();
		ScreenWidth = 0;
		ScreenHeight = 0;
	}

	// 예상 드로우 엘리먼트 수만큼 메모리를 미리 확보한다.
	void Reserve(size_t Count)
	{
		Elements.reserve(Count);
	}
};
