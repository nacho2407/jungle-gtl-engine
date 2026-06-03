#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Time/Timer.h"
#include "Engine/Profiling/Stats/MemoryStats.h"
#include "Engine/Profiling/Stats/PhysicsStats.h"
#include "Engine/Profiling/Stats/ShadowStats.h"
#include "Engine/Profiling/Stats/Stats.h"
#include "Engine/Profiling/GPUProfiler.h"
#include "Viewport/Level/LevelEditorViewportClient.h"
#include "Slate/SWindow.h"
#include "ImGui/imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

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

static const FStatEntry* FindLiveStatEntry(const TArray<FStatEntry>& Snapshot, const char* Category, const char* Name)
{
	for (const FStatEntry& Entry : Snapshot)
	{
		if (Entry.CallCount == 0 || !Entry.Category || !Entry.Name)
		{
			continue;
		}
		if (strcmp(Entry.Category, Category) == 0 && strcmp(Entry.Name, Name) == 0)
		{
			return &Entry;
		}
	}
	return nullptr;
}

static bool AppendTimingLine(TArray<FString>& OutLines, const TArray<FStatEntry>& Snapshot,
	const char* Label, const char* Category, const char* Name)
{
	const FStatEntry* Entry = FindLiveStatEntry(Snapshot, Category, Name);
	if (!Entry)
	{
		return false;
	}

	char Buffer[160] = {};
	snprintf(Buffer, sizeof(Buffer), "%s : %.3f ms  avg %.3f  calls %u",
		Label,
		Entry->LastTime * 1000.0,
		Entry->AvgTime * 1000.0,
		Entry->CallCount);
	OutLines.push_back(FString(Buffer));
	return true;
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

void FOverlayStatSystem::BuildFPSLines(const UEditorEngine& Editor, TArray<FString>& OutLines) const
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

	OutLines.push_back(CachedFPSLine);

	if (bShowPickingTime)
	{
		char Buffer[160] = {};
		snprintf(Buffer, sizeof(Buffer), "Picking Time %.5f ms : Num Attempts %d : Accumulated Time %.5f ms",
			LastPickingTimeMs,
			static_cast<int32>(PickingAttemptCount),
			AccumulatedPickingTimeMs);
		CachedPickingLine = Buffer;
		OutLines.push_back(CachedPickingLine);
	}
}

void FOverlayStatSystem::BuildMemoryLines(TArray<FString>& OutLines) const
{
	char Buffer[128] = {};

	// 할당 횟수 (단위 없음)
	snprintf(Buffer, sizeof(Buffer), "Allocation Count : %u", MemoryStats::GetTotalAllocationCount());
	OutLines.push_back(FString(Buffer));

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
		OutLines.push_back(FString(Buffer));
	}
}

void FOverlayStatSystem::BuildShadowLines(TArray<FString>& OutLines) const
{
#if STATS
	char Buffer[128] = {};

	OutLines.push_back(FString("--- Shadow ---"));

	// Shadow map 메모리
	FormatBytes(Buffer, sizeof(Buffer), "Shadow Map Memory", FShadowStats::ShadowMapMemoryBytes);
	OutLines.push_back(FString(Buffer));

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
	OutLines.push_back(FString(Buffer));

	// Shadow draw call 수
	snprintf(Buffer, sizeof(Buffer), "Shadow Draw Calls : %u", FShadowStats::ShadowDrawCallCount);
	OutLines.push_back(FString(Buffer));

	// 라이트별 shadow caster 수
	snprintf(Buffer, sizeof(Buffer), "Shadow Casters (Spot: %u  Point: %u  Dir: %u)",
		FShadowStats::SpotLightCasterCount,
		FShadowStats::PointLightCasterCount,
		FShadowStats::DirectionalLightCasterCount);
	OutLines.push_back(FString(Buffer));

	// Shadow-casting 라이트 수
	snprintf(Buffer, sizeof(Buffer), "Shadow Lights (Spot: %u  Point: %u  Dir: %u)",
		FShadowStats::SpotLightShadowCount,
		FShadowStats::PointLightShadowCount,
		FShadowStats::DirectionalLightShadowCount);
	OutLines.push_back(FString(Buffer));

	// directional light CSM Shadow map 해상도
	snprintf(Buffer, sizeof(Buffer), "CSM Shadow Map Resolution : %ux%u",
		FShadowStats::ShadowMapResolution, FShadowStats::ShadowMapResolution);
	OutLines.push_back(FString(Buffer));
#else
	OutLines.push_back(FString("Shadow stats unavailable (STATS=0)"));
#endif
}

void FOverlayStatSystem::BuildSkinningLines(TArray<FString>& OutLines) const
{
#if STATS
	char Buffer[160] = {};

	auto MarkStale = [](FSkinningOverlaySample& Sample)
		{
			Sample.bLive = false;
		};
	auto UpdateSample = [](FSkinningOverlaySample& Sample, const FStatEntry& Entry)
		{
			Sample.bValid = true;
			Sample.bLive = true;
			Sample.LastMs = Entry.LastTime * 1000.0;
			Sample.AvgMs = Entry.AvgTime * 1000.0;
			Sample.CallCount = Entry.CallCount;
		};
	auto AppendSample = [&](const char* Label, const FSkinningOverlaySample& Sample)
		{
			if (!Sample.bValid)
			{
				return;
			}

			snprintf(Buffer, sizeof(Buffer), "%s%s : %.3f ms  avg %.3f  calls %u",
				Label,
				Sample.bLive ? "" : " (last)",
				Sample.LastMs,
				Sample.AvgMs,
				Sample.CallCount);
			OutLines.push_back(FString(Buffer));
		};
	auto AppendModeTotal = [&](const char* Label, const FSkinningOverlaySample& A, const FSkinningOverlaySample& B)
		{
			if (!A.bValid || !B.bValid)
			{
				snprintf(Buffer, sizeof(Buffer), "%s : waiting for samples", Label);
				OutLines.push_back(FString(Buffer));
				return;
			}

			snprintf(Buffer, sizeof(Buffer), "%s : %.3f ms",
				Label,
				A.LastMs + B.LastMs);
			OutLines.push_back(FString(Buffer));
		};

	MarkStale(CPUVertexSkinSample);
	MarkStale(GPUMatrixUploadSample);
	MarkStale(SkeletalPreDepthCPUPathSample);
	MarkStale(SkeletalPreDepthGPUPathSample);

	const TArray<FStatEntry>& CPUSnapshot = FStatManager::Get().GetSnapshot();
	for (const FStatEntry& Entry : CPUSnapshot)
	{
		if (Entry.CallCount == 0)
		{
			continue;
		}
		if (!Entry.Category || strcmp(Entry.Category, "Skinning") != 0)
		{
			continue;
		}

		if (Entry.Name && strcmp(Entry.Name, "CPUSkinning_VertexSkin") == 0)
		{
			UpdateSample(CPUVertexSkinSample, Entry);
		}
		else if (Entry.Name && strcmp(Entry.Name, "GPUSkinning_MatrixUpload") == 0)
		{
			UpdateSample(GPUMatrixUploadSample, Entry);
		}
	}

	const TArray<FStatEntry>& GPUSnapshot = FGPUProfiler::Get().GetGPUSnapshot();
	for (const FStatEntry& Entry : GPUSnapshot)
	{
		if (Entry.CallCount == 0)
		{
			continue;
		}
		if (!Entry.Category || strcmp(Entry.Category, "Skinning") != 0)
		{
			continue;
		}

		if (Entry.Name && strcmp(Entry.Name, "SkeletalPreDepth_GPU_CPUPath") == 0)
		{
			UpdateSample(SkeletalPreDepthCPUPathSample, Entry);
		}
		else if (Entry.Name && strcmp(Entry.Name, "SkeletalPreDepth_GPU_GPUPath") == 0)
		{
			UpdateSample(SkeletalPreDepthGPUPathSample, Entry);
		}
	}

	OutLines.push_back(FString("--- CPU Skinning Mode ---"));
	AppendSample("CPU Vertex Skin", CPUVertexSkinSample);
	AppendSample("GPU Skeletal PreDepth (CPU Path)", SkeletalPreDepthCPUPathSample);
	AppendModeTotal("CPU Mode Total (CPU+GPU approx)", CPUVertexSkinSample, SkeletalPreDepthCPUPathSample);

	OutLines.push_back(FString("--- GPU Skinning Mode ---"));
	AppendSample("GPU Matrix Upload CPU", GPUMatrixUploadSample);
	AppendSample("GPU Skeletal PreDepth (GPU Path)", SkeletalPreDepthGPUPathSample);
	AppendModeTotal("GPU Mode Total (CPU+GPU approx)", GPUMatrixUploadSample, SkeletalPreDepthGPUPathSample);

	if (OutLines.empty())
	{
		OutLines.push_back(FString("No Skinning stats this frame"));
	}
#else
	OutLines.push_back(FString("Skinning stats unavailable (STATS=0)"));
#endif
}

void FOverlayStatSystem::BuildParticleLines(const UEditorEngine& Editor,TArray<FString>& OutLines) const
{
	const FLevelEditorViewportClient* ActiveVC = Editor.GetActiveViewport();
	if (!ActiveVC)
	{
		OutLines.push_back(FString("No active viewport"));
		return;
	}

	const FParticleViewportStats& S = ActiveVC->GetParticleStats();

	char Buffer[160] = {};
	snprintf(Buffer, sizeof(Buffer), "Systems : %d", S.ParticleSystemCount);
	OutLines.push_back(Buffer);

	snprintf(Buffer, sizeof(Buffer), "Emitters : %d active / %d total", S.ActiveEmitterCount, S.EmitterCount);
	OutLines.push_back(Buffer);

	snprintf(Buffer, sizeof(Buffer), "Particles : %d active / %d max", S.ActiveParticles, S.MaxParticles);
	OutLines.push_back(Buffer);

	snprintf(Buffer, sizeof(Buffer), "Types : Sprite %d  Mesh %d  Ribbon %d  Beam %d",
		S.SpriteEmitters, S.MeshEmitters, S.RibbonEmitters, S.BeamEmitters);
	OutLines.push_back(Buffer);

	snprintf(Buffer, sizeof(Buffer), "Draw : %d batches  %d sections  %d mesh instances",
		S.DrawBatches, S.DrawSections, S.MeshInstances);
	OutLines.push_back(Buffer);

	FormatBytes(Buffer, sizeof(Buffer), "Particle Memory", S.ParticleMemoryBytes);
	OutLines.push_back(Buffer);
}

void FOverlayStatSystem::BuildPhysXLines(TArray<FString>& OutLines) const
{
#if STATS
	char Buffer[160] = {};

	OutLines.push_back(FString("--- PhysX ---"));

	snprintf(Buffer, sizeof(Buffer), "Scenes : %u", FPhysicsStats::PhysXSceneCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Bodies : %u total  %u static  %u dynamic",
		FPhysicsStats::PhysXBodyCount,
		FPhysicsStats::PhysXStaticBodyCount,
		FPhysicsStats::PhysXDynamicBodyCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Constraints : %u", FPhysicsStats::PhysXConstraintCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Active Actors : %u", FPhysicsStats::PhysXActiveActorCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Vehicles : %u", FPhysicsStats::PhysXVehicleCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Controllers : %u", FPhysicsStats::PhysXControllerCount);
	OutLines.push_back(FString(Buffer));

	OutLines.push_back(FString("--- Timing ---"));
	const TArray<FStatEntry>& CPUSnapshot = FStatManager::Get().GetSnapshot();
	bool bHasTiming = false;
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Prepare CCT", "PhysX", "PhysX_PrepareCCT");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Vehicle Update", "PhysX", "PhysX_VehicleUpdate");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Simulate/Fetch", "PhysX", "PhysX_SimulateFetch");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Body Sync", "PhysX", "PhysX_BodySync");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Raycast", "PhysX", "PhysX_Raycast");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Sweep Sphere", "PhysX", "PhysX_SweepSphere");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Overlap Sphere", "PhysX", "PhysX_OverlapSphere");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Overlap Box", "PhysX", "PhysX_OverlapBox");
	if (!bHasTiming)
	{
		OutLines.push_back(FString("No PhysX timing this frame"));
	}
#else
	OutLines.push_back(FString("PhysX stats unavailable (STATS=0)"));
#endif
}

void FOverlayStatSystem::BuildRagdollLines(TArray<FString>& OutLines) const
{
#if STATS
	char Buffer[160] = {};

	OutLines.push_back(FString("--- Ragdoll ---"));

	snprintf(Buffer, sizeof(Buffer), "Active Ragdolls : %u", FPhysicsStats::ActiveRagdollCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Bodies : %u", FPhysicsStats::RagdollBodyCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Constraints : %u", FPhysicsStats::RagdollConstraintCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Starts : %u attempts  %u success  %u fail",
		FPhysicsStats::RagdollStartAttemptCount,
		FPhysicsStats::RagdollStartSuccessCount,
		FPhysicsStats::RagdollStartFailCount);
	OutLines.push_back(FString(Buffer));

	OutLines.push_back(FString("--- Timing ---"));
	const TArray<FStatEntry>& CPUSnapshot = FStatManager::Get().GetSnapshot();
	bool bHasTiming = false;
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Initialize", "Ragdoll", "Ragdoll_Initialize");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Release", "Ragdoll", "Ragdoll_Release");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Build Local Pose", "Ragdoll", "Ragdoll_BuildLocalPose");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Sync Bones", "Ragdoll", "Ragdoll_SyncBones");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Recovery Blend", "Ragdoll", "Ragdoll_RecoveryBlend");
	if (!bHasTiming)
	{
		OutLines.push_back(FString("No ragdoll timing this frame"));
	}
#else
	OutLines.push_back(FString("Ragdoll stats unavailable (STATS=0)"));
#endif
}

void FOverlayStatSystem::BuildClothLines(TArray<FString>& OutLines) const
{
#if STATS
	char Buffer[160] = {};

	OutLines.push_back(FString("--- Cloth ---"));

	snprintf(Buffer, sizeof(Buffer), "Active Cloth : %u", FPhysicsStats::ActiveClothCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Particles : %u", FPhysicsStats::ClothParticleCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Triangles : %u", FPhysicsStats::ClothTriangleCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Substeps : %u", FPhysicsStats::ClothSubstepCount);
	OutLines.push_back(FString(Buffer));

	snprintf(Buffer, sizeof(Buffer), "Collision : spheres %u  capsules %u  planes %u  convex %u",
		FPhysicsStats::ClothCollisionSphereCount,
		FPhysicsStats::ClothCollisionCapsuleCount,
		FPhysicsStats::ClothCollisionPlaneCount,
		FPhysicsStats::ClothCollisionConvexCount);
	OutLines.push_back(FString(Buffer));

	OutLines.push_back(FString("--- Timing ---"));
	const TArray<FStatEntry>& CPUSnapshot = FStatManager::Get().GetSnapshot();
	bool bHasTiming = false;
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Bone Attachment", "Cloth", "Cloth_BoneAttachment");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "World Collision", "Cloth", "Cloth_WorldCollision");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Collision Gather", "Cloth", "Cloth_CollisionGather");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Update Collision", "Cloth", "Cloth_UpdateCollision");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Simulate", "Cloth", "Cloth_Simulate");
	bHasTiming |= AppendTimingLine(OutLines, CPUSnapshot, "Render Vertices", "Cloth", "Cloth_RenderVertexUpdate");
	if (!bHasTiming)
	{
		OutLines.push_back(FString("No cloth timing this frame"));
	}
#else
	OutLines.push_back(FString("Cloth stats unavailable (STATS=0)"));
#endif
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
	if (bShowSkinning)
	{
		EstimatedLineCount += 4;
	}
	if (bShowParticles)
	{
		EstimatedLineCount += 7;
	}
	if (bShowPhysX)
	{
		EstimatedLineCount += 16;
	}
	if (bShowRagdoll)
	{
		EstimatedLineCount += 10;
	}
	if (bShowCloth)
	{
		EstimatedLineCount += 12;
	}
	OutLines.reserve(EstimatedLineCount);

	TArray<FString> Lines;
	float CurrentY = Layout.StartY;
	auto AppendGroup = [&](const TArray<FString>& GroupLines)
		{
			for (const FString& Line : GroupLines)
			{
				AppendLine(OutLines, CurrentY, Line);
				CurrentY += Layout.LineHeight;
			}
			if (!GroupLines.empty())
			{
				CurrentY += Layout.GroupSpacing;
			}
		};

	if (bShowFPS)
	{
		Lines.clear();
		BuildFPSLines(Editor, Lines);
		AppendGroup(Lines);
	}

	if (bShowMemory)
	{
		Lines.clear();
		BuildMemoryLines(Lines);
		AppendGroup(Lines);
	}

	if (bShowShadow)
	{
		Lines.clear();
		BuildShadowLines(Lines);
		AppendGroup(Lines);
	}

	if (bShowSkinning)
	{
		Lines.clear();
		BuildSkinningLines(Lines);
		AppendGroup(Lines);
	}

	if (bShowParticles)
	{
		Lines.clear();
		BuildParticleLines(Editor, Lines);
		AppendGroup(Lines);
	}

	if (bShowPhysX)
	{
		Lines.clear();
		BuildPhysXLines(Lines);
		AppendGroup(Lines);
	}

	if (bShowRagdoll)
	{
		Lines.clear();
		BuildRagdollLines(Lines);
		AppendGroup(Lines);
	}

	if (bShowCloth)
	{
		Lines.clear();
		BuildClothLines(Lines);
		AppendGroup(Lines);
	}
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatLine> Result;
	BuildLines(Editor, Result);
	return Result;
}

void FOverlayStatSystem::RenderImGui(const UEditorEngine& Editor, const FRect& ViewportRect) const
{
	if (ViewportRect.Width <= 1.0f || ViewportRect.Height <= 1.0f)
	{
		return;
	}

	constexpr float PaddingX = 10.0f;
	constexpr float PaddingY = 30.0f;
	constexpr float WindowGap = 6.0f;
	constexpr float ColumnGap = 8.0f;
	const float ViewportLeft = ViewportRect.X;
	const float ViewportTop = ViewportRect.Y;
	const float ViewportRight = ViewportRect.X + ViewportRect.Width;
	const float ViewportBottom = ViewportRect.Y + ViewportRect.Height;

	float CurrentX = ViewportLeft + PaddingX;
	float CurrentY = ViewportTop + PaddingY;
	float CurrentColumnWidth = 0.0f;

	ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs;

	auto RenderWindow = [&](const char* WindowID, const char* Title, const ImVec4& BgColor, const TArray<FString>& Lines)
		{
			if (Lines.empty())
			{
				return;
			}

			const float EstimatedHeight =
				ImGui::GetTextLineHeightWithSpacing() * (static_cast<float>(Lines.size()) + 1.0f) +
				ImGui::GetStyle().WindowPadding.y * 2.0f;
			if (CurrentY > ViewportTop + PaddingY && CurrentY + EstimatedHeight > ViewportBottom - PaddingY)
			{
				CurrentX += CurrentColumnWidth + ColumnGap;
				CurrentY = ViewportTop + PaddingY;
				CurrentColumnWidth = 0.0f;
			}
			CurrentX = (std::max)(ViewportLeft + PaddingX, (std::min)(CurrentX, ViewportRight - PaddingX - 40.0f));

			ImGui::SetNextWindowPos(ImVec2(CurrentX, CurrentY), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(BgColor.w);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, BgColor);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

			ImGui::Begin(WindowID, nullptr, Flags);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.95f), "%s", Title);
			ImGui::Separator();
			for (const FString& Line : Lines)
			{
				ImGui::TextUnformatted(Line.c_str());
			}
			const ImVec2 WindowSize = ImGui::GetWindowSize();
			ImGui::End();

			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();

			CurrentY += WindowSize.y + WindowGap;
			CurrentColumnWidth = (std::max)(CurrentColumnWidth, WindowSize.x);
		};

	TArray<FString> Lines;
	if (bShowFPS)
	{
		BuildFPSLines(Editor, Lines);
		RenderWindow("##StatFPSOverlay", "Stat FPS", ImVec4(0.05f, 0.09f, 0.12f, 0.62f), Lines);
	}

	if (bShowMemory)
	{
		Lines.clear();
		BuildMemoryLines(Lines);
		RenderWindow("##StatMemoryOverlay", "Stat Memory", ImVec4(0.10f, 0.07f, 0.04f, 0.62f), Lines);
	}

	if (bShowShadow)
	{
		Lines.clear();
		BuildShadowLines(Lines);
		RenderWindow("##StatShadowOverlay", "Stat Shadow", ImVec4(0.08f, 0.05f, 0.12f, 0.62f), Lines);
	}

	if (bShowSkinning)
	{
		Lines.clear();
		BuildSkinningLines(Lines);
		RenderWindow("##StatSkinningOverlay", "Stat Skinning", ImVec4(0.05f, 0.10f, 0.08f, 0.62f), Lines);
	}

	if (bShowParticles)
	{
		Lines.clear();
		BuildParticleLines(Editor, Lines);
		RenderWindow("##StatParticlesOverlay", "Stat Particles", ImVec4(0.08f, 0.08f, 0.03f, 0.62f), Lines);
	}

	if (bShowPhysX)
	{
		Lines.clear();
		BuildPhysXLines(Lines);
		RenderWindow("##StatPhysXOverlay", "Stat PhysX", ImVec4(0.04f, 0.07f, 0.10f, 0.62f), Lines);
	}

	if (bShowRagdoll)
	{
		Lines.clear();
		BuildRagdollLines(Lines);
		RenderWindow("##StatRagdollOverlay", "Stat Ragdoll", ImVec4(0.10f, 0.05f, 0.04f, 0.62f), Lines);
	}

	if (bShowCloth)
	{
		Lines.clear();
		BuildClothLines(Lines);
		RenderWindow("##StatClothOverlay", "Stat Cloth", ImVec4(0.04f, 0.08f, 0.07f, 0.62f), Lines);
	}
}
