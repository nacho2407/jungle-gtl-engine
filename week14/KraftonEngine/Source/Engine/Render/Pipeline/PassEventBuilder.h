#pragma once

#include <functional>
#include "Render/Types/RenderTypes.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/DrawCommandList.h"

class FD3DDevice;

/*
	FPassEvent — 패스 루프 내 Pre/Post 이벤트 훅
	특정 패스 조건이 만족되면 콜백을 실행합니다.
*/
enum class EPassCompare : uint8 { Equal, Less, Greater, LessEqual, GreaterEqual };

struct FPassEvent
{
	ERenderPass    Pass;
	EPassCompare   Compare;
	bool           bOnce;
	bool           bExecuted = false;
	std::function<void()> Fn;

	bool TryExecute(ERenderPass CurPass)
	{
		if (bOnce && bExecuted) return false;

		bool bMatch = false;
		switch (Compare)
		{
		case EPassCompare::Equal:        bMatch = (CurPass == Pass); break;
		case EPassCompare::Less:         bMatch = ((uint32)CurPass < (uint32)Pass); break;
		case EPassCompare::Greater:      bMatch = ((uint32)CurPass > (uint32)Pass); break;
		case EPassCompare::LessEqual:    bMatch = ((uint32)CurPass <= (uint32)Pass); break;
		case EPassCompare::GreaterEqual: bMatch = ((uint32)CurPass >= (uint32)Pass); break;
		}

		if (bMatch) { Fn(); if (bOnce) bExecuted = true; }
		return bMatch;
	}
};

/*
	FPassEventBuilder — 패스 루프의 Pre/Post 이벤트를 등록합니다.
	각 논리 그룹(PreDepth, DepthCopy, MRT, StencilCopy, SceneColorCopy)을
	독립 메서드로 분리하여 확장 시 메서드 하나만 추가하면 됩니다.
*/
class FPassEventBuilder
{
public:
	void Build(FD3DDevice& Device,
		const FFrameContext& Frame, FStateCache& Cache,
		TArray<FPassEvent>& OutPreEvents,
		TArray<FPassEvent>& OutPostEvents);

private:
	// PreDepth: DSV-only 렌더링 → RTV 복귀
	void RegisterPreDepthEvents(ID3D11DeviceContext* Ctx,
		const FFrameContext& Frame, FStateCache& Cache,
		TArray<FPassEvent>& Pre, TArray<FPassEvent>& Post);

	// DepthCopy + MRT: Opaque 진입 전 Depth 복사 + Normal MRT 세팅
	void RegisterDepthCopyAndMRTEvents(ID3D11DeviceContext* Ctx,
		const FFrameContext& Frame, FStateCache& Cache,
		TArray<FPassEvent>& Pre, TArray<FPassEvent>& Post);

	// StencilCopy: PostProcess 진입 전 Stencil 복사
	void RegisterStencilCopyEvents(ID3D11DeviceContext* Ctx,
		const FFrameContext& Frame, FStateCache& Cache,
		TArray<FPassEvent>& Pre);

	// SceneColorCopy: FXAA 진입 전 화면 복사
	void RegisterSceneColorCopyEvents(ID3D11DeviceContext* Ctx,
		const FFrameContext& Frame, FStateCache& Cache,
		TArray<FPassEvent>& Pre);
};
