#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"

void FOverlayStatSystem::RecordPickingAttempt(const FPickingFrameStats& Stats)
{
	LastPickingStats = Stats;
	AccumulatedPickingStats.TotalMs += Stats.TotalMs;
	AccumulatedPickingStats.GizmoMs += Stats.GizmoMs;
	AccumulatedPickingStats.WorldBVHMs += Stats.WorldBVHMs;
	AccumulatedPickingStats.NarrowPhaseMs += Stats.NarrowPhaseMs;
	AccumulatedPickingStats.MeshBVHMs += Stats.MeshBVHMs;
	AccumulatedPickingStats.WorldInternalNodesVisited += Stats.WorldInternalNodesVisited;
	AccumulatedPickingStats.WorldLeafNodesVisited += Stats.WorldLeafNodesVisited;
	AccumulatedPickingStats.PrimitiveAABBTests += Stats.PrimitiveAABBTests;
	AccumulatedPickingStats.PrimitiveAABBHits += Stats.PrimitiveAABBHits;
	AccumulatedPickingStats.PrimitiveNarrowPhaseCalls += Stats.PrimitiveNarrowPhaseCalls;
	AccumulatedPickingStats.MeshInternalNodesVisited += Stats.MeshInternalNodesVisited;
	AccumulatedPickingStats.MeshLeafPacketsTested += Stats.MeshLeafPacketsTested;
	AccumulatedPickingStats.MeshTriangleLanesTested += Stats.MeshTriangleLanesTested;
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
			const double SafeAttemptCount = NumAttempts > 0 ? static_cast<double>(NumAttempts) : 1.0;
			snprintf(Buffer, sizeof(Buffer), "Picking %.4f ms : Attempts %d : Accumulated %.4f ms",
				LastPickingStats.TotalMs, NumAttempts, AccumulatedPickingStats.TotalMs);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Stages Last Gizmo %.4f / World %.4f / Narrow %.4f / Mesh %.4f ms",
				LastPickingStats.GizmoMs, LastPickingStats.WorldBVHMs,
				LastPickingStats.NarrowPhaseMs, LastPickingStats.MeshBVHMs);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Stages Acc Gizmo %.4f / World %.4f / Narrow %.4f / Mesh %.4f ms",
				AccumulatedPickingStats.GizmoMs, AccumulatedPickingStats.WorldBVHMs,
				AccumulatedPickingStats.NarrowPhaseMs, AccumulatedPickingStats.MeshBVHMs);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Stages Avg Gizmo %.4f / World %.4f / Narrow %.4f / Mesh %.4f ms",
				AccumulatedPickingStats.GizmoMs / SafeAttemptCount,
				AccumulatedPickingStats.WorldBVHMs / SafeAttemptCount,
				AccumulatedPickingStats.NarrowPhaseMs / SafeAttemptCount,
				AccumulatedPickingStats.MeshBVHMs / SafeAttemptCount);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "World Last Nodes %u Internal %u Leaf / Primitive %u tested %u hit / Calls %u",
				LastPickingStats.WorldInternalNodesVisited, LastPickingStats.WorldLeafNodesVisited,
				LastPickingStats.PrimitiveAABBTests, LastPickingStats.PrimitiveAABBHits,
				LastPickingStats.PrimitiveNarrowPhaseCalls);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "World Avg Nodes %.2f Internal %.2f Leaf / Primitive %.2f tested %.2f hit / Calls %.2f",
				static_cast<double>(AccumulatedPickingStats.WorldInternalNodesVisited) / SafeAttemptCount,
				static_cast<double>(AccumulatedPickingStats.WorldLeafNodesVisited) / SafeAttemptCount,
				static_cast<double>(AccumulatedPickingStats.PrimitiveAABBTests) / SafeAttemptCount,
				static_cast<double>(AccumulatedPickingStats.PrimitiveAABBHits) / SafeAttemptCount,
				static_cast<double>(AccumulatedPickingStats.PrimitiveNarrowPhaseCalls) / SafeAttemptCount);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Mesh Last Nodes %u Internal / Packets %u / Triangle Lanes %u",
				LastPickingStats.MeshInternalNodesVisited, LastPickingStats.MeshLeafPacketsTested,
				LastPickingStats.MeshTriangleLanesTested);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer), "Mesh Avg Nodes %.2f Internal / Packets %.2f / Triangle Lanes %.2f",
				static_cast<double>(AccumulatedPickingStats.MeshInternalNodesVisited) / SafeAttemptCount,
				static_cast<double>(AccumulatedPickingStats.MeshLeafPacketsTested) / SafeAttemptCount,
				static_cast<double>(AccumulatedPickingStats.MeshTriangleLanesTested) / SafeAttemptCount);
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
