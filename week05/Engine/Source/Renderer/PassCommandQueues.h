#pragma once

#include "Renderer/MeshPassProcessor.h"

struct ENGINE_API FPassCommandQueues
{
	TArray<FMeshDrawCommand> BasePassCommands;
	TArray<FMeshDrawCommand> OverlayPassCommands;
	TArray<FMeshDrawCommand> UIPassCommands;
	TArray<FMeshDrawCommand> OutlineMaskCommands;
	TArray<FMeshDrawCommand> OutlineCompositeCommands;

	void Reset()
	{
		BasePassCommands.clear();
		OverlayPassCommands.clear();
		UIPassCommands.clear();
		OutlineMaskCommands.clear();
		OutlineCompositeCommands.clear();
	}

	TArray<FMeshDrawCommand>& GetQueue(ERenderPass InRenderPass)
	{
		switch (InRenderPass)
		{
		case ERenderPass::NoDepth:
			return OverlayPassCommands;
		case ERenderPass::UI:
			return UIPassCommands;
		case ERenderPass::World:
		default:
			return BasePassCommands;
		}
	}

	const TArray<FMeshDrawCommand>& GetQueue(ERenderPass InRenderPass) const
	{
		switch (InRenderPass)
		{
		case ERenderPass::NoDepth:
			return OverlayPassCommands;
		case ERenderPass::UI:
			return UIPassCommands;
		case ERenderPass::World:
		default:
			return BasePassCommands;
		}
	}
};
