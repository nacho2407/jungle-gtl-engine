#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"

TArray<FOverlayStatGroup> FOverlayStatSystem::BuildGroups(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatGroup> Groups;

	if (bShowFPS)
	{
		FOverlayStatGroup Group;

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

		/*{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Memory Allocated : %u", MemoryStats::GetTotalAllocationBytes());
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Times Allocated : %u", MemoryStats::GetTotalAllocationCount());
			Group.Lines.push_back(FString(Buffer));
		}*/

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "PixelShader Memory : %llu bytes", static_cast<unsigned long long>(MemoryStats::GetPixelShaderMemory()));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexShader Memory : %llu bytes", static_cast<unsigned long long>(MemoryStats::GetVertexShaderMemory()));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Texture Memory : %llu bytes", static_cast<unsigned long long>(MemoryStats::GetTextureMemory()));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexBuffer Memory : %llu bytes", static_cast<unsigned long long>(MemoryStats::GetVertexBufferMemory()));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "IndexBuffer Memory : %llu bytes", static_cast<unsigned long long>(MemoryStats::GetIndexBufferMemory()));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "StaticMesh CPU Memory : %llu bytes", static_cast<unsigned long long>(MemoryStats::GetStaticMeshCPUMemory()));
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	return Groups;
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatLine> Result;
	const TArray<FOverlayStatGroup> Groups = BuildGroups(Editor);

	float CurrentY = Layout.StartY;

	for (const FOverlayStatGroup& Group : Groups)
	{
		for (size_t i = 0; i < Group.Lines.size(); ++i)
		{
			FOverlayStatLine Line;
			Line.Text = Group.Lines[i];
			Line.ScreenPosition = FVector2(
				Layout.StartX,
				CurrentY + static_cast<float>(i) * Layout.LineHeight
			);

			Result.push_back(std::move(Line));
		}

		if (!Group.Lines.empty())
		{
			CurrentY += static_cast<float>(Group.Lines.size()) * Layout.LineHeight;
			CurrentY += Layout.GroupSpacing;
		}
	}

	return Result;
}
