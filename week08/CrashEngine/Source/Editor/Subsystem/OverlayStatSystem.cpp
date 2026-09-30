// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Profiling/Stats.h"
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

static bool UsesLightingPass(EViewMode ViewMode)
{
    switch (ViewMode)
    {
    case EViewMode::Lit_Gouraud:
    case EViewMode::Unlit:
    case EViewMode::WorldNormal:
    case EViewMode::Wireframe:
    case EViewMode::SceneDepth:
        return false;
    default:
        return true;
    }
}

FString FOverlayStatSystem::GetDisplayTitle() const
{
    if (bShowFPS)
    {
        return FString("Performance Statistics");
    }
    if (bShowPickingTime)
    {
        return FString("Picking Statistics");
    }
    if (bShowMemory)
    {
        return FString("Memory Statistics");
    }
    if (bShowShadow)
    {
        return FString("Shadow Statistics");
    }
    if (bShowLightCull)
    {
        return FString("Light Culling Statistics");
    }

    return FString("Statistics");
}

void FOverlayStatSystem::AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Label, const FString& Value) const
{
    FOverlayStatLine Line;
    Line.Label = Label;
    Line.Value = Value;
    Line.ScreenPosition = FVector2(Layout.StartX, Y);
    OutLines.push_back(std::move(Line));
}

void FOverlayStatSystem::AppendSection(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Label) const
{
    FOverlayStatLine Line;
    Line.Label = Label;
    Line.ScreenPosition = FVector2(Layout.StartX, Y);
    Line.bIsSectionHeader = true;
    OutLines.push_back(std::move(Line));
}

void FOverlayStatSystem::RecordPickingAttempt(double ElapsedMs)
{
    LastPickingTimeMs = ElapsedMs;
    AccumulatedPickingTimeMs += ElapsedMs;
    ++PickingAttemptCount;
}

void FOverlayStatSystem::ClearDisplayFlags()
{
    bShowFPS = false;
    bShowPickingTime = false;
    bShowMemory = false;
    bShowShadow = false;
    bShowLightCull = false;
}

void FOverlayStatSystem::ShowFPS(bool bEnable)
{
    ClearDisplayFlags();
    bShowFPS = bEnable;
    FLightCullStats::SetEnabled(false);
}

void FOverlayStatSystem::ShowPickingTime(bool bEnable)
{
    ClearDisplayFlags();
    bShowPickingTime = bEnable;
    FLightCullStats::SetEnabled(false);
}

void FOverlayStatSystem::ShowMemory(bool bEnable)
{
    ClearDisplayFlags();
    bShowMemory = bEnable;
    FLightCullStats::SetEnabled(false);
}

void FOverlayStatSystem::ShowShadow(bool bEnable)
{
    ClearDisplayFlags();
    bShowShadow = bEnable;
    FLightCullStats::SetEnabled(false);
}

void FOverlayStatSystem::ShowLightCull(bool bEnable)
{
    ClearDisplayFlags();
    bShowLightCull = bEnable;
    FLightCullStats::SetEnabled(bEnable);
    if (!bEnable)
    {
        FLightCullStats::ResetSample();
    }
}

void FOverlayStatSystem::HideAll()
{
    ClearDisplayFlags();
    FLightCullStats::SetEnabled(false);
    FLightCullStats::ResetSample();
}

void FOverlayStatSystem::BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const
{
    OutLines.clear();

    uint32 EstimatedLineCount = 0;
    if (bShowFPS)
    {
        EstimatedLineCount += 3;
    }
    if (bShowPickingTime)
    {
        EstimatedLineCount += 3;
    }
    if (bShowMemory)
    {
        EstimatedLineCount += 8;
    }
    if (bShowShadow)
    {
        EstimatedLineCount += 15;
    }
    if (bShowLightCull)
    {
        EstimatedLineCount += 2;
    }
    OutLines.reserve(EstimatedLineCount);

    float CurrentY = Layout.StartY;
    if (bShowFPS)
    {
        const FTimer* Timer = Editor.GetTimer();
        const float FPS = Timer ? Timer->GetFPS() : 0.0f;
        const float MS = Timer ? Timer->GetFrameTimeMs() : 0.0f;
        const float DeltaTime = Timer ? Timer->GetDeltaTime() : 0.0f;
        const float AverageWindowSeconds = 1.5f;
        const float Alpha = (DeltaTime > 0.0f)
            ? std::clamp(DeltaTime / AverageWindowSeconds, 0.0f, 1.0f)
            : 1.0f;

        if (!bHasAveragedFPS)
        {
            AveragedFPS = FPS;
            AveragedFrameTimeMs = MS;
            bHasAveragedFPS = true;
        }
        else
        {
            AveragedFPS += (FPS - AveragedFPS) * Alpha;
            AveragedFrameTimeMs += (MS - AveragedFrameTimeMs) * Alpha;
        }

        char Buffer[128] = {};
        snprintf(Buffer, sizeof(Buffer), "%.1f", AveragedFPS);
        CachedFPSLine = Buffer;
        AppendLine(OutLines, CurrentY, FString("FPS (1.5s Avg)"), CachedFPSLine);
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%.2f ms", AveragedFrameTimeMs);
        AppendLine(OutLines, CurrentY, FString("Frame Time (1.5s Avg)"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%.1f", FPS);
        AppendLine(OutLines, CurrentY, FString("FPS (Current)"), FString(Buffer));
        CurrentY += Layout.LineHeight + Layout.GroupSpacing;
    }

    if (bShowPickingTime)
    {
        char Buffer[160] = {};
        snprintf(Buffer, sizeof(Buffer), "%.5f ms", LastPickingTimeMs);
        AppendLine(OutLines, CurrentY, FString("Last Attempt"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%d", static_cast<int32>(PickingAttemptCount));
        AppendLine(OutLines, CurrentY, FString("Attempt Count"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%.5f ms", AccumulatedPickingTimeMs);
        CachedPickingLine = Buffer;
        AppendLine(OutLines, CurrentY, FString("Accumulated"), CachedPickingLine);
        CurrentY += Layout.LineHeight + Layout.GroupSpacing;
    }

    if (bShowMemory)
    {
        char Buffer[128] = {};

        // 할당 횟수 (단위 없음)
        snprintf(Buffer, sizeof(Buffer), "%u", MemoryStats::GetTotalAllocationCount());
        AppendLine(OutLines, CurrentY, FString("Allocation Count"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        // 바이트 단위 메모리 — 자동 단위 변환 (B/KB/MB/GB)
        struct
        {
            const char* Label;
            uint64 Bytes;
        } MemEntries[] = {
            { "Total Allocated", MemoryStats::GetTotalAllocationBytes() },
            { "PixelShader Memory", MemoryStats::GetPixelShaderMemory() },
            { "VertexShader Memory", MemoryStats::GetVertexShaderMemory() },
            { "VertexBuffer Memory", MemoryStats::GetVertexBufferMemory() },
            { "IndexBuffer Memory", MemoryStats::GetIndexBufferMemory() },
            { "StaticMesh CPU Memory", MemoryStats::GetStaticMeshCPUMemory() },
            { "Texture Memory", MemoryStats::GetTextureMemory() },
        };

        for (const auto& Entry : MemEntries)
        {
            FormatBytes(Buffer, sizeof(Buffer), Entry.Label, Entry.Bytes);
            const char* Separator = strstr(Buffer, " : ");
            if (Separator)
            {
                const int32 LabelLength = static_cast<int32>(Separator - Buffer);
                AppendLine(
                    OutLines,
                    CurrentY,
                    FString(Buffer, Buffer + LabelLength),
                    FString(Separator + 3));
            }
            else
            {
                AppendLine(OutLines, CurrentY, FString(Entry.Label), FString(Buffer));
            }
            CurrentY += Layout.LineHeight;
        }

    }

    if (bShowShadow)
    {
        char Buffer[128] = {};
        FShadowAtlasBudgetStats ShadowStats = {};
        if (FRenderPass* Pass = Editor.GetRenderer().GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
        {
            ShadowStats = static_cast<FShadowMapPass*>(Pass)->GetShadowAtlasBudgetStats();
        }

        const FCollectedLights& CollectedLights = Editor.GetRenderer().GetCollectedLights();
        uint32 VisibleDirectionalLights = 0;
        uint32 VisibleSpotLights = 0;
        uint32 VisiblePointLights = 0;
        for (const FLightProxy* Light : CollectedLights.VisibleLightProxies)
        {
            if (!Light)
            {
                continue;
            }

            if (Light->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Directional))
            {
                ++VisibleDirectionalLights;
            }
            else if (Light->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Spot))
            {
                ++VisibleSpotLights;
            }
            else if (Light->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Point))
            {
                ++VisiblePointLights;
            }
        }

        AppendSection(OutLines, CurrentY, FString("Atlas"));
        CurrentY += Layout.LineHeight;

        FormatBytes(Buffer, sizeof(Buffer), "Used Memory", ShadowStats.UsedAtlasMemoryBytes);
        if (const char* Separator = strstr(Buffer, " : "))
        {
            AppendLine(OutLines, CurrentY, FString(Buffer, Buffer + static_cast<int32>(Separator - Buffer)), FString(Separator + 3));
        }
        CurrentY += Layout.LineHeight;

        FormatBytes(Buffer, sizeof(Buffer), "Resident Memory", ShadowStats.ResidentAtlasMemoryBytes);
        if (const char* Separator = strstr(Buffer, " : "))
        {
            AppendLine(OutLines, CurrentY, FString(Buffer, Buffer + static_cast<int32>(Separator - Buffer)), FString(Separator + 3));
        }
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%u / %u", ShadowStats.UsedPageCount, ShadowAtlas::MaxPages);
        AppendLine(OutLines, CurrentY, FString("Used Pages"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%u", ShadowStats.UsedSliceCount);
        AppendLine(OutLines, CurrentY, FString("Used Slices"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%.1f%%", ShadowStats.UsedAreaPercent);
        AppendLine(OutLines, CurrentY, FString("Used Area"), FString(Buffer));
        CurrentY += Layout.LineHeight + Layout.GroupSpacing;

        AppendSection(OutLines, CurrentY, FString("Moments"));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%s (%u pages)", ShadowStats.MomentResidentPageCount > 0 ? "Resident" : "Disabled", ShadowStats.MomentResidentPageCount);
        AppendLine(OutLines, CurrentY, FString("Moment Resources"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        FormatBytes(Buffer, sizeof(Buffer), "Moment Resident", ShadowStats.ResidentMomentMemoryBytes);
        if (const char* Separator = strstr(Buffer, " : "))
        {
            AppendLine(OutLines, CurrentY, FString(Buffer, Buffer + static_cast<int32>(Separator - Buffer)), FString(Separator + 3));
        }
        CurrentY += Layout.LineHeight;

        FormatBytes(Buffer, sizeof(Buffer), "Moment Used", ShadowStats.UsedMomentMemoryBytes);
        if (const char* Separator = strstr(Buffer, " : "))
        {
            AppendLine(OutLines, CurrentY, FString(Buffer, Buffer + static_cast<int32>(Separator - Buffer)), FString(Separator + 3));
        }
        CurrentY += Layout.LineHeight;

        CurrentY += Layout.GroupSpacing;
        AppendSection(OutLines, CurrentY, FString("Lights"));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "Dir %u  Spot %u  Point %u", VisibleDirectionalLights, VisibleSpotLights, VisiblePointLights);
        AppendLine(OutLines, CurrentY, FString("Visible Count"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "Dir %u  Spot %u  Point %u",
            ShadowStats.DirectionalShadowLightCount,
            ShadowStats.SpotShadowLightCount,
            ShadowStats.PointShadowLightCount);
        AppendLine(OutLines, CurrentY, FString("Shadowed Count"), FString(Buffer));
        CurrentY += Layout.LineHeight + Layout.GroupSpacing;

        AppendSection(OutLines, CurrentY, FString("Allocation"));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%u", ShadowStats.FailedAllocationCount);
        AppendLine(OutLines, CurrentY, FString("Alloc Failures"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%u", ShadowStats.EvictedShadowCount);
        AppendLine(OutLines, CurrentY, FString("Evicted"), FString(Buffer));
        CurrentY += Layout.LineHeight;
    }

    if (bShowLightCull)
    {
        const FLevelEditorViewportClient* ActiveViewport = Editor.GetActiveViewport();
        const EViewMode ActiveViewMode = ActiveViewport ? ActiveViewport->GetRenderOptions().ViewMode : EViewMode::Unlit;
        if (!UsesLightingPass(ActiveViewMode))
        {
            AppendLine(OutLines, CurrentY, FString("Lighting GPU"), FString("N/A"));
            CurrentY += Layout.LineHeight;

            AppendLine(OutLines, CurrentY, FString("Light Evaluations"), FString("N/A"));
            CurrentY += Layout.LineHeight;
            return;
        }

        if (!FLightCullStats::HasSample())
        {
            AppendLine(OutLines, CurrentY, FString("Lighting GPU"), FString("Not measured"));
            CurrentY += Layout.LineHeight;

            AppendLine(OutLines, CurrentY, FString("Light Evaluations"), FString("Not measured"));
            CurrentY += Layout.LineHeight;
            return;
        }

        char Buffer[128] = {};
        snprintf(Buffer, sizeof(Buffer), "%.3f ms", FLightCullStats::GetGPUTimeMs());
        AppendLine(OutLines, CurrentY, FString("Lighting GPU"), FString(Buffer));
        CurrentY += Layout.LineHeight;

        snprintf(Buffer, sizeof(Buffer), "%u", FLightCullStats::GetEvaluationCount());
        AppendLine(OutLines, CurrentY, FString("Light Evaluations"), FString(Buffer));
        CurrentY += Layout.LineHeight;
    }
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
    TArray<FOverlayStatLine> Result;
    BuildLines(Editor, Result);
    return Result;
}
