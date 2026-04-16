#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Stats.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include <cstdio>

void FOverlayStatSystem::AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Text) const
{
	FOverlayStatLine Line;
	Line.Text = Text;
	Line.ScreenPosition = FVector2(Layout.StartX, Y);
	OutLines.push_back(std::move(Line));
}

void FOverlayStatSystem::RecordPickingAttempt(double ElapsedMs)
{
	LastPickingTimeMs = ElapsedMs;
	AccumulatedPickingTimeMs += ElapsedMs;
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
			const double PickingTimeMS = LastPickingTimeMs;
			const double AccumulatedTime = AccumulatedPickingTimeMs;
			snprintf(Buffer, sizeof(Buffer), "Picking Time %.5f ms : Num Attempts %d : Accumulated Time %.5f ms",
				PickingTimeMS, NumAttempts, AccumulatedTime);
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
	if (bShowDecal)
	{
		// Use previous-frame decal stats so overlay (collected before render) shows meaningful values
		const FDecalFrameStats& DecalStats = FDecalStats::Previous;

		FOverlayStatGroup Group;

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Visible Decals");
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	return Groups;
}

void FOverlayStatSystem::BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const
{
	OutLines.clear();

	uint32 EstimatedLineCount = 0;
	if (bShowFPS)
	{
		++EstimatedLineCount;
	}
	if (bShowPickingTime)
	{
		++EstimatedLineCount;
	}
	if (bShowDecal)
	{
		EstimatedLineCount += 10;
	}
	if (bShowMemory)
	{
		EstimatedLineCount += 6;
	}
	OutLines.reserve(EstimatedLineCount);

	float CurrentY = Layout.StartY;
	if (bShowFPS)
	{
		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		const float MS = FPS > 0.0f ? 1000.0f / FPS : 0.0f;

		char Buffer[128] = {};
		snprintf(Buffer, sizeof(Buffer), "FPS : %.1f (%.2f ms)", FPS, MS);
		CachedFPSLine = Buffer;
		AppendLine(OutLines, CurrentY, CachedFPSLine);
		CurrentY += Layout.LineHeight + Layout.GroupSpacing;
	}

	if (bShowPickingTime)
	{
		char Buffer[160] = {};
		snprintf(Buffer, sizeof(Buffer), "Picking Time %.5f ms : Num Attempts %d : Accumulated Time %.5f ms",
			LastPickingTimeMs,
			static_cast<int32>(PickingAttemptCount),
			AccumulatedPickingTimeMs);
		CachedPickingLine = Buffer;
		AppendLine(OutLines, CurrentY, CachedPickingLine);
		CurrentY += Layout.LineHeight + Layout.GroupSpacing;
	}

	if (bShowMemory)
	{
		constexpr int32 MemoryLineCount = 6;
		char Buffer[128] = {};
		const double ValuesKB[MemoryLineCount] = {
			static_cast<double>(MemoryStats::GetPixelShaderMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetVertexShaderMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetVertexBufferMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetIndexBufferMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetStaticMeshCPUMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetTextureMemory() / 1024.0f),
		};

		const char* Labels[MemoryLineCount] = {
			"PixelShader Memory",
			"VertexShader Memory",
			"VertexBuffer Memory",
			"IndexBuffer Memory",
			"StaticMesh CPU Memory",
			"Texture Memory",
		};

		for (int32 Index = 0; Index < MemoryLineCount; ++Index)
		{
			snprintf(Buffer, sizeof(Buffer), "%s : %.2f KB", Labels[Index], ValuesKB[Index]);
			AppendLine(OutLines, CurrentY, FString(Buffer));
			CurrentY += Layout.LineHeight;
		}
	}

	if (bShowDecal)
	{
		// read previous-frame stats so overlay shows values available before current render
		const FDecalFrameStats& DecalStats = FDecalStats::Previous;
		const TArray<FStatEntry>& CPUSnapshot = FStatManager::Get().GetSnapshot();
		const FStatEntry* CollectEntry = FindStatEntry(CPUSnapshot, "Decal::Collect", "Decal");
		const FStatEntry* RenderEntry = FindStatEntry(CPUSnapshot, "Decal::Render", "Decal");
		const double CollectTimeMs = CollectEntry ? CollectEntry->TotalTime * 1000.0 : 0.0;
		const double RenderTimeMs = RenderEntry ? RenderEntry->TotalTime * 1000.0 : 0.0;
		const double TotalTimeMs = CollectTimeMs + RenderTimeMs;

		constexpr int32 DecalCountLineCount = 7;
		char Buffer[128] = {};
		const int32 Values[DecalCountLineCount] = {
			(DecalStats.VisibleDecals),
			(DecalStats.VisibleReceivers),
			(DecalStats.BroadCandidates),
			(DecalStats.UniqueCandidates),
			(DecalStats.SATAccepted),
			(DecalStats.SubmittedDraws),
			(DecalStats.RenderedDraws)
		};

		const char* Labels[DecalCountLineCount] = {
			"Visible Decals",
			"Visible Receivers",
			"Broad Candidates",
			"Unique Candidates",
			"SAT Accepted",
			"Submitted Draws",
			"Rendered Draws",
		};

		for (int32 Index = 0; Index < DecalCountLineCount; ++Index)
		{
			snprintf(Buffer, sizeof(Buffer), "%s : %d", Labels[Index], Values[Index]);
			AppendLine(OutLines, CurrentY, FString(Buffer));
			CurrentY += Layout.LineHeight;
		}

		snprintf(Buffer, sizeof(Buffer), "Collect Time : %.3f ms", CollectTimeMs);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		snprintf(Buffer, sizeof(Buffer), "Render Time : %.3f ms", RenderTimeMs);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		snprintf(Buffer, sizeof(Buffer), "Total Time : %.3f ms", TotalTimeMs);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;
	}
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatLine> Result;
	BuildLines(Editor, Result);
	return Result;
}
