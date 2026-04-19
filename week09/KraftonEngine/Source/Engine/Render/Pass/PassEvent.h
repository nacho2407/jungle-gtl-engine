#pragma once

#include <functional>
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Pass/DrawCommandList.h"

class FViewModeRenderPipeline;
class FViewModeSurfaceResources;
struct FFrameContext;

enum class EPassCompare : uint8 { Equal, Less, Greater, LessEqual, GreaterEqual };

struct FPassEvent
{
	ERenderPass    Pass = ERenderPass::Opaque;
	EPassCompare   Compare = EPassCompare::Equal;
	bool           bOnce = true;
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

void BuildDefaultPassEvents(
	TArray<FPassEvent>& OutPrePassEvents,
	ID3D11DeviceContext* Context,
	const FFrameContext& Frame,
	FStateCache& Cache,
	const FViewModeRenderPipeline* ActiveViewPipeline,
	FViewModeSurfaceResources* ActiveViewSurfaces);
