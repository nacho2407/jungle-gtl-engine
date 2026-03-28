#include "Editor/Viewport/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Core/Timer.h"
#include "Engine/Object/EngineStatics.h"

TArray<FOverlayStatGroup> FOverlayStatSystem::BuildGroups(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatGroup> Groups;

	if (bShowFPS)
	{
		FOverlayStatGroup Group;
		Group.StartY = FPSStartY;

		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		const float MS = FPS > 0.0f ? 1000.0f / FPS : 0.0f;
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "FPS : %.1f", FPS);
			Group.Lines.push_back(FString(Buffer));
		}
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Frame Time : %.2f ms", MS);
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	if (bShowMemory)
	{
		FOverlayStatGroup Group;
		Group.StartY = MemoryStartY;
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Memory Allocated : %u", EngineStatics::GetTotalAllocationBytes());
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Times Allocated : %u", EngineStatics::GetTotalAllocationCount());
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "PixelShader Memory : %u", EngineStatics::GetPixelShaderMemory());
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexShader Memory : %u", EngineStatics::GetVertexShaderMemory());
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	return Groups;
}