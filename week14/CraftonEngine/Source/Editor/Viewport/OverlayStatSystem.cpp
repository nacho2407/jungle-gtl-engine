#include "Editor/Viewport/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Core/Timer.h"
#include "Engine/Object/EngineStatics.h"

TArray<FString> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FString> Lines;

	if (bShowFPS)
	{
		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		const float MS = FPS > 0.0f ? 1000.0f / FPS : 0.0f;
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "FPS : %.1f", FPS);
			Lines.push_back(FString(Buffer));
		}
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Frame Time : %.2f ms", MS);
			Lines.push_back(FString(Buffer));
		}
	}

	if (bShowMemory)
	{
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Memory Allocated : %u", EngineStatics::GetTotalAllocationBytes());
			Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Times Allocated : %u", EngineStatics::GetTotalAllocationCount());
			Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "PixelShader Memory : %u", EngineStatics::GetPixelShaderMemory());
			Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexShader Memory : %u", EngineStatics::GetVertexShaderMemory());
			Lines.push_back(FString(Buffer));
		}
	}

	return Lines;
}