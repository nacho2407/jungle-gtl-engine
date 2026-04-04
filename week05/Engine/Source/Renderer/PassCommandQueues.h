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

	TArray<FMeshDrawCommand>& GetQueue(EMeshPass InMeshPass)
	{
		switch (InMeshPass)
		{
		case EMeshPass::Overlay:
			return OverlayPassCommands;
		case EMeshPass::UI:
			return UIPassCommands;
		case EMeshPass::OutlineMask:
			return OutlineMaskCommands;
		case EMeshPass::OutlineComposite:
			return OutlineCompositeCommands;
		case EMeshPass::Base:
		default:
			return BasePassCommands;
		}
	}

	const TArray<FMeshDrawCommand>& GetQueue(EMeshPass InMeshPass) const
	{
		switch (InMeshPass)
		{
		case EMeshPass::Overlay:
			return OverlayPassCommands;
		case EMeshPass::UI:
			return UIPassCommands;
		case EMeshPass::OutlineMask:
			return OutlineMaskCommands;
		case EMeshPass::OutlineComposite:
			return OutlineCompositeCommands;
		case EMeshPass::Base:
		default:
			return BasePassCommands;
		}
	}
};
