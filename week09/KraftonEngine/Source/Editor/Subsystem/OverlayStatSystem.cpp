#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"

void FOverlayStatSystem::RecordPickingAttempt(const FPickingFrameStats& Stats)
{
	LastPickingStats = Stats;
	AccumulatedPickingTimeMs += Stats.TotalMs;
	++PickingAttemptCount;
}

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
			snprintf(Buffer, sizeof(Buffer), "FPS : %.1f (%.2f ms)", FPS, MS);
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	if (bShowPickingTime)
	{
		FOverlayStatGroup Group;

		{
			char Buffer[128] = {};
			const int32 NumAttempts = static_cast<int32>(PickingAttemptCount);
			snprintf(Buffer, sizeof(Buffer), "Picking %.4f ms : Attempts %d : Accumulated %.4f ms",
				LastPickingStats.TotalMs, NumAttempts, AccumulatedPickingTimeMs);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Stages Gizmo %.4f / World %.4f / Narrow %.4f / Mesh %.4f ms",
				LastPickingStats.GizmoMs, LastPickingStats.WorldBVHMs,
				LastPickingStats.NarrowPhaseMs, LastPickingStats.MeshBVHMs);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "World Nodes %u Internal %u Leaf / Primitive %u tested %u hit / Calls %u",
				LastPickingStats.WorldInternalNodesVisited, LastPickingStats.WorldLeafNodesVisited,
				LastPickingStats.PrimitiveAABBTests, LastPickingStats.PrimitiveAABBHits,
				LastPickingStats.PrimitiveNarrowPhaseCalls);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Mesh Nodes %u Internal / Packets %u / Triangle Lanes %u",
				LastPickingStats.MeshInternalNodesVisited, LastPickingStats.MeshLeafPacketsTested,
				LastPickingStats.MeshTriangleLanesTested);
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
			snprintf(Buffer, sizeof(Buffer), "PixelShader Memory : %.2f KB", static_cast<double>(MemoryStats::GetPixelShaderMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexShader Memory : %.2f KB", static_cast<double>(MemoryStats::GetVertexShaderMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexBuffer Memory : %.2f KB", static_cast<double>(MemoryStats::GetVertexBufferMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "IndexBuffer Memory : %.2f KB", static_cast<double>(MemoryStats::GetIndexBufferMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "StaticMesh CPU Memory : %.2f KB", static_cast<double>(MemoryStats::GetStaticMeshCPUMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Texture Memory : %.2f KB", static_cast<double>(MemoryStats::GetTextureMemory() / 1024.0f));
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
