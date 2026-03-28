#include "Editor/Viewport/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Core/Timer.h"
#include "Engine/Object/EngineStatics.h"

TArray<FString> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FString> Lines;

	if (bShowFPS)
	{
		char Buffer[128] = {};
		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		snprintf(Buffer, sizeof(Buffer), "FPS : %.1f", FPS);
		Lines.push_back(FString(Buffer));
	}

	if (bShowMemory)
	{
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Memory Allocated : %d", EngineStatics::GetTotalAllocationBytes());
			Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Times Allocated : %d", EngineStatics::GetTotalAllocationCount());
			Lines.push_back(FString(Buffer));
		}
	}

	return Lines;
}