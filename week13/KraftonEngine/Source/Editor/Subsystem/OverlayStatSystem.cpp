#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Engine/Profiling/ShadowStats.h"
#include "Engine/Profiling/GPUProfiler.h"
#include <cstdio>

// バイト数を適切な単位 (B / KB / MB / GB) に変換して文字列化
static int FormatBytes(char* Buffer, int32 BufferSize, const char* Label, uint64 Bytes)
{
	const double B = static_cast<double>(Bytes);
	const double KB = B / 1024.0;
	const double MB = KB / 1024.0;
	const double GB = MB / 1024.0;

	if (GB >= 1.0)
		return snprintf(Buffer, BufferSize, "%s : %.2f GB", Label, GB);
	if (MB >= 1.0)
		return snprintf(Buffer, BufferSize, "%s : %.2f MB", Label, MB);
	if (KB >= 1.0)
		return snprintf(Buffer, BufferSize, "%s : %.2f KB", Label, KB);
	return snprintf(Buffer, BufferSize, "%s : %llu B", Label, static_cast<unsigned long long>(Bytes));
}

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
	if (bShowMemory)
	{
		EstimatedLineCount += 8;
	}
	if (bShowShadow)
	{
		EstimatedLineCount += 8;
	}
	OutLines.reserve(EstimatedLineCount);

	float CurrentY = Layout.StartY;
	if (bShowFPS)
	{
		const FTimer* Timer = Editor.GetTimer();
		if (Timer)
		{
			constexpr double FPSAverageWindowSeconds = 0.3;
			const double CurrentTime = Timer->GetTotalTime();

			if (!bFPSAverageInitialized)
			{
				FPSAverageWindowStartTime = CurrentTime;
				FPSAccumulatedFrameTimeMs = 0.0;
				FPSAccumulatedFrameCount = 0;
				bFPSAverageInitialized = true;
			}

			FPSAccumulatedFrameTimeMs += Timer->GetFrameTimeMs();
			++FPSAccumulatedFrameCount;

			const double WindowElapsed = CurrentTime - FPSAverageWindowStartTime;
			if (WindowElapsed >= FPSAverageWindowSeconds && FPSAccumulatedFrameCount > 0)
			{
				const float AverageMS = static_cast<float>(FPSAccumulatedFrameTimeMs / FPSAccumulatedFrameCount);
				const float AverageFPS = AverageMS > 0.0f ? 1000.0f / AverageMS : 0.0f;

				char Buffer[128] = {};
				snprintf(Buffer, sizeof(Buffer), "FPS : %.1f (%.2f ms)", AverageFPS, AverageMS);
				CachedFPSLine = Buffer;

				FPSAverageWindowStartTime = CurrentTime;
				FPSAccumulatedFrameTimeMs = 0.0;
				FPSAccumulatedFrameCount = 0;
			}
		}
		else
		{
			CachedFPSLine = "FPS : 0.0 (0.00 ms)";
			bFPSAverageInitialized = false;
			FPSAccumulatedFrameTimeMs = 0.0;
			FPSAccumulatedFrameCount = 0;
		}

		if (CachedFPSLine.empty())
		{
			CachedFPSLine = "FPS : 0.0 (0.00 ms)";
		}

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
		char Buffer[128] = {};

		// 할당 횟수 (단위 없음)
		snprintf(Buffer, sizeof(Buffer), "Allocation Count : %u", MemoryStats::GetTotalAllocationCount());
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// 바이트 단위 메모리 — 자동 단위 변환 (B/KB/MB/GB)
		struct { const char* Label; uint64 Bytes; } MemEntries[] = {
			{ "Total Allocated",       MemoryStats::GetTotalAllocationBytes() },
			{ "PixelShader Memory",    MemoryStats::GetPixelShaderMemory() },
			{ "VertexShader Memory",   MemoryStats::GetVertexShaderMemory() },
			{ "VertexBuffer Memory",   MemoryStats::GetVertexBufferMemory() },
			{ "IndexBuffer Memory",    MemoryStats::GetIndexBufferMemory() },
			{ "StaticMesh CPU Memory", MemoryStats::GetStaticMeshCPUMemory() },
			{ "Texture Memory",        MemoryStats::GetTextureMemory() },
		};

		for (const auto& Entry : MemEntries)
		{
			FormatBytes(Buffer, sizeof(Buffer), Entry.Label, Entry.Bytes);
			AppendLine(OutLines, CurrentY, FString(Buffer));
			CurrentY += Layout.LineHeight;
		}
	}

#if STATS
	if (bShowShadow)
	{
		char Buffer[128] = {};

		AppendLine(OutLines, CurrentY, FString("--- Shadow ---"));
		CurrentY += Layout.LineHeight;

		// Shadow map 메모리
		FormatBytes(Buffer, sizeof(Buffer), "Shadow Map Memory", FShadowStats::ShadowMapMemoryBytes);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// GPU 시간 (GPUProfiler snapshot에서 "ShadowMapPass" 검색)
		const TArray<FStatEntry>& GPUSnapshot = FGPUProfiler::Get().GetGPUSnapshot();
		double ShadowGpuMs = 0.0;
		for (const FStatEntry& Entry : GPUSnapshot)
		{
			if (Entry.Name && strcmp(Entry.Name, "ShadowMapPass") == 0)
			{
				ShadowGpuMs = Entry.LastTime * 1000.0;
				break;
			}
		}
		snprintf(Buffer, sizeof(Buffer), "Shadow GPU Time : %.3f ms", ShadowGpuMs);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// Shadow draw call 수
		snprintf(Buffer, sizeof(Buffer), "Shadow Draw Calls : %u", FShadowStats::ShadowDrawCallCount);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// 라이트별 shadow caster 수
		snprintf(Buffer, sizeof(Buffer), "Shadow Casters (Spot: %u  Point: %u  Dir: %u)",
			FShadowStats::SpotLightCasterCount,
			FShadowStats::PointLightCasterCount,
			FShadowStats::DirectionalLightCasterCount);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// Shadow-casting 라이트 수
		snprintf(Buffer, sizeof(Buffer), "Shadow Lights (Spot: %u  Point: %u  Dir: %u)",
			FShadowStats::SpotLightShadowCount,
			FShadowStats::PointLightShadowCount,
			FShadowStats::DirectionalLightShadowCount);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// directional light CSM Shadow map 해상도
		snprintf(Buffer, sizeof(Buffer), "CSM Shadow Map Resolution : %ux%u",
			FShadowStats::ShadowMapResolution, FShadowStats::ShadowMapResolution);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;
	}
#endif
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatLine> Result;
	BuildLines(Editor, Result);
	return Result;
}
