#include "Editor/UI/EditorDetailsPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include "ImGui/imgui.h"

#include "Component/LightComponent.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Submission/Atlas/LightShadowAllocationRegistry.h"

#include <cmath>

namespace
{
struct FShadowAtlasOwnerInfo
{
    UActorComponent* Component = nullptr;
    FLightProxy* LightProxy = nullptr;
    int32 CascadeIndex = -1;
    int32 FaceIndex = -1;
};

static FString BuildComponentDisplayLabel(UActorComponent* Comp)
{
    if (!Comp)
    {
        return "None";
    }

    const FString TypeName = Comp->GetClass()->GetName();
    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
    {
        Name = TypeName;
    }
    return TypeName + "(" + Name + ")";
}

static bool MatchesShadowAllocation(const FShadowMapData& A, const FShadowMapData& B)
{
    return A.bAllocated == B.bAllocated &&
           A.AtlasPageIndex == B.AtlasPageIndex &&
           A.SliceIndex == B.SliceIndex &&
           A.Rect.X == B.Rect.X &&
           A.Rect.Y == B.Rect.Y &&
           A.Rect.Width == B.Rect.Width &&
           A.Rect.Height == B.Rect.Height &&
           A.ViewportRect.X == B.ViewportRect.X &&
           A.ViewportRect.Y == B.ViewportRect.Y &&
           A.ViewportRect.Width == B.ViewportRect.Width &&
           A.ViewportRect.Height == B.ViewportRect.Height;
}

static const FMatrix* FindShadowAtlasAllocationViewProj(UWorld* World, const FShadowMapData& Allocation)
{
    if (!World || !Allocation.bAllocated)
    {
        return nullptr;
    }

    const TArray<FLightProxy*>& LightProxies = World->GetScene().GetLightProxies();
    for (FLightProxy* LightProxy : LightProxies)
    {
        if (!LightProxy || !LightProxy->Owner)
        {
            continue;
        }

        if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy->GetCascadeShadowMapData())
        {
            const uint32 CascadeCount = std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades));
            for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
            {
                if (MatchesShadowAllocation(CascadeShadowMapData->Cascades[CascadeIndex], Allocation))
                {
                    return &CascadeShadowMapData->CascadeViewProj[CascadeIndex];
                }
            }
        }

        if (const FShadowMapData* SpotShadowMapData = LightProxy->GetSpotShadowMapData())
        {
            if (MatchesShadowAllocation(*SpotShadowMapData, Allocation))
            {
                return &LightProxy->LightViewProj;
            }
        }

        if (const FCubeShadowMapData* CubeShadowMapData = LightProxy->GetCubeShadowMapData())
        {
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                if (MatchesShadowAllocation(CubeShadowMapData->Faces[FaceIndex], Allocation))
                {
                    return &CubeShadowMapData->FaceViewProj[FaceIndex];
                }
            }
        }
    }

    return nullptr;
}

static FShadowAtlasOwnerInfo FindShadowAtlasOwnerInfo(UWorld* World, const FShadowMapData& Allocation)
{
    if (!World || !Allocation.bAllocated)
    {
        return {};
    }

    const TArray<FLightProxy*>& LightProxies = World->GetScene().GetLightProxies();
    for (FLightProxy* LightProxy : LightProxies)
    {
        if (!LightProxy || !LightProxy->Owner)
        {
            continue;
        }

        if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy->GetCascadeShadowMapData())
        {
            const uint32 CascadeCount = std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades));
            for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
            {
                if (MatchesShadowAllocation(CascadeShadowMapData->Cascades[CascadeIndex], Allocation))
                {
                    FShadowAtlasOwnerInfo Info = {};
                    Info.Component = LightProxy->Owner;
                    Info.LightProxy = LightProxy;
                    Info.CascadeIndex = static_cast<int32>(CascadeIndex);
                    return Info;
                }
            }
        }

        if (const FShadowMapData* SpotShadowMapData = LightProxy->GetSpotShadowMapData())
        {
            if (MatchesShadowAllocation(*SpotShadowMapData, Allocation))
            {
                FShadowAtlasOwnerInfo Info = {};
                Info.Component = LightProxy->Owner;
                Info.LightProxy = LightProxy;
                return Info;
            }
        }

        if (const FCubeShadowMapData* CubeShadowMapData = LightProxy->GetCubeShadowMapData())
        {
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                if (MatchesShadowAllocation(CubeShadowMapData->Faces[FaceIndex], Allocation))
                {
                    FShadowAtlasOwnerInfo Info = {};
                    Info.Component = LightProxy->Owner;
                    Info.LightProxy = LightProxy;
                    Info.FaceIndex = static_cast<int32>(FaceIndex);
                    return Info;
                }
            }
        }
    }

    return {};
}

static FString BuildShadowAtlasOwnerLabel(const FShadowAtlasOwnerInfo& Info)
{
    if (!Info.Component)
    {
        return "Unresolved allocation";
    }

    FString Label = BuildComponentDisplayLabel(Info.Component);
    if (AActor* OwnerActor = Info.Component->GetOwner())
    {
        FString ActorName = OwnerActor->GetFName().ToString();
        if (ActorName.empty())
        {
            ActorName = OwnerActor->GetClass()->GetName();
        }
        Label = ActorName + " / " + Label;
    }

    if (Info.CascadeIndex >= 0)
    {
        Label += " / Cascade " + std::to_string(Info.CascadeIndex);
    }
    if (Info.FaceIndex >= 0)
    {
        Label += " / Face " + std::to_string(Info.FaceIndex);
    }

    return Label;
}

static ImU32 GetShadowAtlasDebugColor(uint32 Resolution)
{
    switch (Resolution)
    {
    case 256: return IM_COL32(89, 199, 255, 255);
    case 512: return IM_COL32(110, 230, 140, 255);
    case 1024: return IM_COL32(255, 214, 89, 255);
    case 2048: return IM_COL32(255, 150, 80, 255);
    case 4096: return IM_COL32(255, 90, 90, 255);
    default: return IM_COL32(220, 220, 220, 255);
    }
}

static void DrawHatchedBand(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 FillColor, ImU32 LineColor)
{
    if (!DrawList || Max.x <= Min.x || Max.y <= Min.y)
    {
        return;
    }

    DrawList->AddRectFilled(Min, Max, FillColor);
    DrawList->PushClipRect(Min, Max, true);
    constexpr float HatchSpacing = 8.0f;
    const float Height = Max.y - Min.y;
    for (float X = Min.x - Height; X < Max.x; X += HatchSpacing)
    {
        DrawList->AddLine(ImVec2(X, Max.y), ImVec2(X + Height, Min.y), LineColor, 1.0f);
    }
    DrawList->PopClipRect();
}

static void DrawInactiveShadowAtlasPadding(ImDrawList* DrawList, const ImVec2& RectMin, const ImVec2& RectMax, const ImVec2& ViewMin, const ImVec2& ViewMax)
{
    const ImU32 FillColor = IM_COL32(48, 112, 220, 84);
    const ImU32 LineColor = IM_COL32(132, 194, 255, 220);

    constexpr float PaddingEpsilon = 0.5f;
    if (std::fabs(RectMin.x - ViewMin.x) <= PaddingEpsilon &&
        std::fabs(RectMin.y - ViewMin.y) <= PaddingEpsilon &&
        std::fabs(RectMax.x - ViewMax.x) <= PaddingEpsilon &&
        std::fabs(RectMax.y - ViewMax.y) <= PaddingEpsilon)
    {
        return;
    }

    DrawHatchedBand(DrawList, RectMin, ImVec2(RectMax.x, ViewMin.y), FillColor, LineColor);
    DrawHatchedBand(DrawList, ImVec2(RectMin.x, ViewMax.y), RectMax, FillColor, LineColor);
    DrawHatchedBand(DrawList, ImVec2(RectMin.x, ViewMin.y), ImVec2(ViewMin.x, ViewMax.y), FillColor, LineColor);
    DrawHatchedBand(DrawList, ImVec2(ViewMax.x, ViewMin.y), ImVec2(RectMax.x, ViewMax.y), FillColor, LineColor);
}

static ImVec2 InsetRectMin(const ImVec2& Min, const ImVec2& Max, float Inset)
{
    return ImVec2((std::min)(Min.x + Inset, Max.x), (std::min)(Min.y + Inset, Max.y));
}

static ImVec2 InsetRectMax(const ImVec2& Min, const ImVec2& Max, float Inset)
{
    return ImVec2((std::max)(Max.x - Inset, Min.x), (std::max)(Max.y - Inset, Min.y));
}

static const char* GetShadowLightTypeLabel(uint32 LightType)
{
    switch (static_cast<ELightType>(LightType))
    {
    case ELightType::Directional: return "Directional";
    case ELightType::Point: return "Point";
    case ELightType::Spot: return "Spot";
    case ELightType::Ambient: return "Ambient";
    default: return "Unknown";
    }
}

static uint32 GetLightCascadeCountForEvictionDebug(const FLightProxy& LightProxy)
{
    if (LightProxy.LightProxyInfo.LightType != static_cast<uint32>(ELightType::Directional))
    {
        return 1;
    }

    if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy.GetCascadeShadowMapData())
    {
        return std::max(1u, std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades)));
    }

    return std::max(1u, std::min(LightProxy.GetCascadeCountSetting(), static_cast<uint32>(ShadowAtlas::MaxCascades)));
}

static bool CompareShadowMapDataStable(const FShadowMapData& A, const FShadowMapData& B)
{
    if (A.AtlasPageIndex != B.AtlasPageIndex) return A.AtlasPageIndex < B.AtlasPageIndex;
    if (A.SliceIndex != B.SliceIndex) return A.SliceIndex < B.SliceIndex;
    if (A.Rect.Y != B.Rect.Y) return A.Rect.Y < B.Rect.Y;
    if (A.Rect.X != B.Rect.X) return A.Rect.X < B.Rect.X;
    if (A.Rect.Width != B.Rect.Width) return A.Rect.Width < B.Rect.Width;
    return A.Rect.Height < B.Rect.Height;
}

static uint64 HashShadowAllocationsStable(TArray<FShadowMapData> Allocations)
{
    std::sort(Allocations.begin(), Allocations.end(), CompareShadowMapDataStable);

    uint64 Hash = 1469598103934665603ull;
    auto Mix = [&Hash](uint32 Value)
    {
        Hash ^= static_cast<uint64>(Value);
        Hash *= 1099511628211ull;
    };

    Mix(static_cast<uint32>(Allocations.size()));
    for (const FShadowMapData& Allocation : Allocations)
    {
        Mix(Allocation.AtlasPageIndex);
        Mix(Allocation.SliceIndex);
        Mix(Allocation.Resolution);
        Mix(Allocation.Rect.X);
        Mix(Allocation.Rect.Y);
        Mix(Allocation.Rect.Width);
        Mix(Allocation.Rect.Height);
        Mix(Allocation.ViewportRect.X);
        Mix(Allocation.ViewportRect.Y);
        Mix(Allocation.ViewportRect.Width);
        Mix(Allocation.ViewportRect.Height);
        Mix(Allocation.NodeIndex);
    }

    return Hash;
}

static void NormalizeShadowAllocationsForDisplay(TArray<FShadowMapData>& Allocations)
{
    std::sort(Allocations.begin(), Allocations.end(), CompareShadowMapDataStable);
}

static bool HasInactiveShadowAtlasPadding(const FShadowMapData& Allocation)
{
    return Allocation.Rect.X != Allocation.ViewportRect.X ||
           Allocation.Rect.Y != Allocation.ViewportRect.Y ||
           Allocation.Rect.Width != Allocation.ViewportRect.Width ||
           Allocation.Rect.Height != Allocation.ViewportRect.Height;
}
}

void RenderEditorShadowAtlasDebugWindow(FEditorDetailsPanel& DetailsPanel)
{
    if (!GEngine)
    {
        return;
    }

    FRenderPass* Pass = GEngine->GetRenderer().GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass);
    FShadowMapPass* ShadowPass = static_cast<FShadowMapPass*>(Pass);
    if (!ShadowPass)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(700.0f, 760.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Shadow Atlas"))
    {
        ImGui::End();
        return;
    }

    const uint32 PageCount = ShadowPass->GetShadowAtlasPageCount();
    if (PageCount == 0)
    {
        ImGui::TextDisabled("No shadow atlas page has been allocated yet.");
        ImGui::End();
        return;
    }

    DetailsPanel.SelectedShadowAtlasPage = std::clamp(DetailsPanel.SelectedShadowAtlasPage, 0, static_cast<int32>(PageCount) - 1);

    static const char* AtlasPreviewModeLabels[] = { "Raw Depth", "Linearized Depth", "Moments" };
    int32 AtlasPreviewModeIndex = static_cast<int32>(DetailsPanel.ShadowAtlasDebugPreviewMode);
    const FShadowAtlasBudgetStats ShadowStats = ShadowPass->GetShadowAtlasBudgetStats();
    const EShadowFilterMethod ShadowFilterMethod = GetShadowFilterMethod();
    const bool bSupportsMoments =
        ShadowFilterMethod == EShadowFilterMethod::VSM ||
        ShadowFilterMethod == EShadowFilterMethod::ESM;
    const bool bMomentUnavailable = DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::Moments &&
                                    (!bSupportsMoments || ShadowStats.MomentResidentPageCount == 0);

    const float StatusWidth = ImGui::GetContentRegionAvail().x;
    const bool bWideStatusRow = StatusWidth >= 640.0f;
    const char* MomentStateLabel = !bSupportsMoments
        ? "Disabled"
        : (ShadowStats.MomentResidentPageCount > 0 ? "Resident" : "Pending");

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.72f, 0.78f, 1.0f));
    if (bWideStatusRow)
    {
        const float ColumnWidth = StatusWidth * 0.30f;

        ImGui::Text("Atlas");
        ImGui::SameLine();
        ImGui::TextDisabled("%u x %u", ShadowPass->GetShadowAtlasSize(), ShadowPass->GetShadowAtlasSize());

        ImGui::SameLine(ColumnWidth);
        ImGui::Text("Pages");
        ImGui::SameLine();
        ImGui::TextDisabled("%u / %u", PageCount, ShadowAtlas::MaxPages);

        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ImGui::Text("Filter");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", GetShadowFilterMethodName(ShadowFilterMethod));

        ImGui::SameLine(ColumnWidth);
        ImGui::Text("Moment");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", MomentStateLabel);
    }
    else
    {
        const float HalfWidth = (std::max)(180.0f, (StatusWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f);

        ImGui::BeginGroup();
        ImGui::Text("Atlas");
        ImGui::SameLine();
        ImGui::TextDisabled("%u x %u", ShadowPass->GetShadowAtlasSize(), ShadowPass->GetShadowAtlasSize());
        ImGui::EndGroup();

        ImGui::SameLine(HalfWidth);
        ImGui::BeginGroup();
        ImGui::Text("Pages");
        ImGui::SameLine();
        ImGui::TextDisabled("%u / %u", PageCount, ShadowAtlas::MaxPages);
        ImGui::EndGroup();

        ImGui::BeginGroup();
        ImGui::Text("Filter");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", GetShadowFilterMethodName(ShadowFilterMethod));
        ImGui::EndGroup();

        ImGui::SameLine(HalfWidth);
        ImGui::BeginGroup();
        ImGui::Text("Moment");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", MomentStateLabel);
        ImGui::EndGroup();
    }
    if (ImGui::IsItemHovered() && ShadowStats.MomentResidentPageCount > 0)
    {
        ImGui::BeginTooltip();
        ImGui::Text("Resident Pages: %u", ShadowStats.MomentResidentPageCount);
        ImGui::Text("Resident Memory: %.1f MB", static_cast<double>(ShadowStats.ResidentMomentMemoryBytes) / (1024.0 * 1024.0));
        ImGui::Text("Used Memory: %.1f MB", static_cast<double>(ShadowStats.UsedMomentMemoryBytes) / (1024.0 * 1024.0));
        ImGui::EndTooltip();
    }
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::Moments)
    {
        if (!bSupportsMoments)
        {
            ImGui::TextDisabled("Moments are available only when the shadow filter is VSM or ESM.");
            ImGui::End();
            return;
        }
        else if (ShadowStats.MomentResidentPageCount == 0)
        {
            ImGui::TextDisabled("Moment unavailable");
            ImGui::End();
            return;
        }
    }
    else if (DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::LinearizedDepth)
    {
        ImGui::TextDisabled("Linearized atlas preview uses the same per-allocation preview path as the light details panel.");
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::TextDisabled("Legend");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.35f, 0.78f, 1.0f, 1.0f), "256");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.43f, 0.90f, 0.55f, 1.0f), "512");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.35f, 1.0f), "1024");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.59f, 0.31f, 1.0f), "2048");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "4096");
    ImGui::SameLine();
    ImGui::TextDisabled("  |  white = viewport, color = buddy block");

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::BeginGroup();
    ImGui::SeparatorText("Controls");
    const float ControlsLabelWidth = 56.0f;
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Preview");
    ImGui::SameLine(ControlsLabelWidth);
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::Combo("##AtlasPreviewMode", &AtlasPreviewModeIndex, AtlasPreviewModeLabels, IM_ARRAYSIZE(AtlasPreviewModeLabels)))
    {
        DetailsPanel.ShadowAtlasDebugPreviewMode = static_cast<EShadowDepthPreviewMode>(std::clamp(AtlasPreviewModeIndex, 0, 2));
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Page");
    ImGui::SameLine(ControlsLabelWidth);
    for (uint32 PageIndex = 0; PageIndex < PageCount; ++PageIndex)
    {
        if (PageIndex > 0)
        {
            ImGui::SameLine();
        }

        char ButtonLabel[32] = {};
        sprintf_s(ButtonLabel, "Page %u", PageIndex);
        if (ImGui::Selectable(ButtonLabel, DetailsPanel.SelectedShadowAtlasPage == static_cast<int32>(PageIndex), 0, ImVec2(78.0f, 0.0f)))
        {
            DetailsPanel.SelectedShadowAtlasPage = static_cast<int32>(PageIndex);
        }
    }
    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    const ImGuiStyle& Style = ImGui::GetStyle();
    const float RightEdgeReserve = Style.ScrollbarSize + Style.WindowPadding.x + 28.0f;
    const float ColumnSpacing = Style.ItemSpacing.x;
    const float AvailableGridWidth = (std::max)(260.0f, ImGui::GetContentRegionAvail().x - RightEdgeReserve);
    const float ChildWidth = (std::max)(190.0f, (AvailableGridWidth - ColumnSpacing) * 0.5f);
    const float ChildInnerPadding = (Style.WindowPadding.x * 2.0f) + 18.0f;
    const float ImageSize = (std::max)(140.0f, ChildWidth - ChildInnerPadding);
    const float LabelHeight = ImGui::GetTextLineHeightWithSpacing();
    const float ChildHeight = ImageSize + LabelHeight + 44.0f;
    UWorld* World = GEngine->GetWorld();
    for (uint32 SliceIndex = 0; SliceIndex < ShadowAtlas::SliceCount; ++SliceIndex)
    {
        ID3D11ShaderResourceView* SliceSRV = nullptr;
        if (!bMomentUnavailable)
        {
            if (DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::Moments)
            {
                SliceSRV = ShadowPass->GetShadowPageSliceMomentPreviewSRV(static_cast<uint32>(DetailsPanel.SelectedShadowAtlasPage), SliceIndex);
            }
            else
            {
                SliceSRV = ShadowPass->GetShadowPageSlicePreviewSRV(static_cast<uint32>(DetailsPanel.SelectedShadowAtlasPage), SliceIndex);
            }
        }

        char ChildLabel[40] = {};
        sprintf_s(ChildLabel, "Slice %u##AtlasSlice%u", SliceIndex, SliceIndex);
        ImGui::BeginChild(ChildLabel, ImVec2(ChildWidth, ChildHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.70f, 0.76f, 1.0f));
        ImGui::Text("Slice %u", SliceIndex);
        ImGui::PopStyleColor();

        const ImVec2 ImageMin = ImGui::GetCursorScreenPos();
        const ImVec2 ImageMax(ImageMin.x + ImageSize, ImageMin.y + ImageSize);
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const bool bUseSliceImage = DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::RawDepth;

        if (SliceSRV && bUseSliceImage)
        {
            DrawList->AddImageQuad(
                reinterpret_cast<ImTextureID>(SliceSRV),
                ImageMin,
                ImVec2(ImageMax.x, ImageMin.y),
                ImageMax,
                ImVec2(ImageMin.x, ImageMax.y),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                ImVec2(0.0f, 1.0f));
        }
        ImGui::Dummy(ImVec2(ImageSize, ImageSize));

        if (bMomentUnavailable || (DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::Moments && !SliceSRV))
        {
            ImGui::SetCursorScreenPos(ImVec2(ImageMin.x + 8.0f, ImageMin.y + 8.0f));
            ImGui::TextDisabled("Moment unavailable");
        }

        DrawList->AddRect(ImageMin, ImageMax, IM_COL32(160, 160, 160, 255), 0.0f, 0, 1.0f);

        if (!bMomentUnavailable)
        {
            TArray<FShadowMapData> SliceAllocations;
            ShadowPass->GetShadowPageSliceAllocations(static_cast<uint32>(DetailsPanel.SelectedShadowAtlasPage), SliceIndex, SliceAllocations);
            NormalizeShadowAllocationsForDisplay(SliceAllocations);
            FShadowAtlasOwnerInfo HoveredOwnerInfo = {};
            const FShadowMapData* HoveredAllocation = nullptr;
            ImVec2 HoveredRectMin = {};
            ImVec2 HoveredRectMax = {};
            uint32 InactivePaddingAllocationCount = 0;

            for (const FShadowMapData& Allocation : SliceAllocations)
            {
                const float Scale = ImageSize / static_cast<float>(ShadowPass->GetShadowAtlasSize());
                const ImVec2 RawRectMin(
                    ImageMin.x + static_cast<float>(Allocation.Rect.X) * Scale,
                    ImageMin.y + static_cast<float>(Allocation.Rect.Y) * Scale);
                const ImVec2 RawRectMax(
                    RawRectMin.x + static_cast<float>(Allocation.Rect.Width) * Scale,
                    RawRectMin.y + static_cast<float>(Allocation.Rect.Height) * Scale);
                const ImVec2 RawViewMin(
                    ImageMin.x + static_cast<float>(Allocation.ViewportRect.X) * Scale,
                    ImageMin.y + static_cast<float>(Allocation.ViewportRect.Y) * Scale);
                const ImVec2 RawViewMax(
                    RawViewMin.x + static_cast<float>(Allocation.ViewportRect.Width) * Scale,
                    RawViewMin.y + static_cast<float>(Allocation.ViewportRect.Height) * Scale);
                const ImVec2 RectMin = InsetRectMin(RawRectMin, RawRectMax, 0.75f);
                const ImVec2 RectMax = InsetRectMax(RawRectMin, RawRectMax, 0.75f);
                const ImVec2 ViewMin = InsetRectMin(RawViewMin, RawViewMax, 1.5f);
                const ImVec2 ViewMax = InsetRectMax(RawViewMin, RawViewMax, 1.5f);

                const ImU32 Color = GetShadowAtlasDebugColor(Allocation.Resolution);
                if (HasInactiveShadowAtlasPadding(Allocation))
                {
                    ++InactivePaddingAllocationCount;
                }

                if (HasInactiveShadowAtlasPadding(Allocation))
                {
                    DrawInactiveShadowAtlasPadding(DrawList, RawRectMin, RawRectMax, RawViewMin, RawViewMax);
                }
                DrawList->AddRectFilled(RawRectMin, RawRectMax, IM_COL32(0, 0, 0, 32));

                if (DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::LinearizedDepth ||
                    DetailsPanel.ShadowAtlasDebugPreviewMode == EShadowDepthPreviewMode::Moments)
                {
                    const FShadowAtlasOwnerInfo AllocationOwnerInfo = FindShadowAtlasOwnerInfo(World, Allocation);
                    if (const FMatrix* AllocationViewProj = FindShadowAtlasAllocationViewProj(World, Allocation))
                    {
                        if (ID3D11ShaderResourceView* AllocationPreviewSRV = ShadowPass->GetShadowDebugPreviewSRV(
                                Allocation,
                                *AllocationViewProj,
                                DetailsPanel.ShadowAtlasDebugPreviewMode,
                                AllocationOwnerInfo.LightProxy ? AllocationOwnerInfo.LightProxy->ShadowESMExponent : 40.0f,
                                GEngine->GetRenderer().GetFD3DDevice().GetDevice(),
                                GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext()))
                        {
                            DrawList->AddImageQuad(
                                reinterpret_cast<ImTextureID>(AllocationPreviewSRV),
                                RawViewMin,
                                ImVec2(RawViewMax.x, RawViewMin.y),
                                RawViewMax,
                                ImVec2(RawViewMin.x, RawViewMax.y),
                                ImVec2(0.0f, 0.0f),
                                ImVec2(1.0f, 0.0f),
                                ImVec2(1.0f, 1.0f),
                                ImVec2(0.0f, 1.0f));
                        }
                    }
                }

                DrawList->AddRect(RectMin, RectMax, Color, 0.0f, 0, 2.0f);
                DrawList->AddRect(ViewMin, ViewMax, IM_COL32(255, 255, 255, 140), 0.0f, 0, 1.0f);

                char RectLabel[24] = {};
                sprintf_s(RectLabel, "%u", Allocation.Resolution);
                const float RectWidth = RectMax.x - RectMin.x;
                const float RectHeight = RectMax.y - RectMin.y;
                const ImVec2 LabelSize = ImGui::CalcTextSize(RectLabel);
                if (RectWidth >= LabelSize.x + 8.0f && RectHeight >= LabelSize.y + 8.0f)
                {
                    DrawList->AddText(ImVec2(RectMin.x + 4.0f, RectMin.y + 4.0f), Color, RectLabel);
                }

                const bool bHovered = ImGui::IsMouseHoveringRect(RawRectMin, RawRectMax) && ImGui::IsWindowHovered();
                if (bHovered)
                {
                    HoveredAllocation = &Allocation;
                    HoveredOwnerInfo = FindShadowAtlasOwnerInfo(World, Allocation);
                    HoveredRectMin = RawRectMin;
                    HoveredRectMax = RawRectMax;
                }
            }

            if (HoveredAllocation)
            {
                DrawList->AddRect(HoveredRectMin, HoveredRectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 3.0f);

                ImGui::BeginTooltip();
                ImGui::TextUnformatted(BuildShadowAtlasOwnerLabel(HoveredOwnerInfo).c_str());
                ImGui::Separator();
                ImGui::Text("Resolution: %u", HoveredAllocation->Resolution);
                ImGui::Text("Atlas Page: %u  Slice: %u", HoveredAllocation->AtlasPageIndex, HoveredAllocation->SliceIndex);
                ImGui::Text("Rect: (%u, %u) %ux%u", HoveredAllocation->Rect.X, HoveredAllocation->Rect.Y, HoveredAllocation->Rect.Width, HoveredAllocation->Rect.Height);
                ImGui::Text("Viewport: (%u, %u) %ux%u",
                    HoveredAllocation->ViewportRect.X,
                    HoveredAllocation->ViewportRect.Y,
                    HoveredAllocation->ViewportRect.Width,
                    HoveredAllocation->ViewportRect.Height);
                if (HoveredOwnerInfo.LightProxy)
                {
                    const FLightShadowAllocationRegistry::FShadowEvictionDebugInfo DebugInfo =
                        FLightShadowAllocationRegistry::BuildEvictionDebugInfo(
                            *HoveredOwnerInfo.LightProxy,
                            HoveredAllocation->Resolution,
                            GetLightCascadeCountForEvictionDebug(*HoveredOwnerInfo.LightProxy),
                            HoveredOwnerInfo.LightProxy->LightProxyInfo.LightType);
                    ImGui::Separator();
                    ImGui::Text("Eviction Score: %.6f", DebugInfo.FinalScore);
                    ImGui::TextDisabled(
                        "Priority %.2f / Cost %.0f",
                        DebugInfo.Priority,
                        DebugInfo.Cost);
                    ImGui::TextDisabled(
                        "%s | Type %.1f + Intensity %.1f + Casters %.1f + Radius %.1f + DirBonus %.1f",
                        GetShadowLightTypeLabel(DebugInfo.LightType),
                        DebugInfo.TypeWeight,
                        DebugInfo.IntensityWeight,
                        DebugInfo.CasterWeight,
                        DebugInfo.RadiusWeight,
                        DebugInfo.DirectionalBonus);
                }
                if (HasInactiveShadowAtlasPadding(*HoveredAllocation))
                {
                    ImGui::TextDisabled("This allocation contains unused padded area.");
                }
                if (HoveredOwnerInfo.Component)
                {
                    ImGui::TextDisabled("Click to focus this component in Details.");
                }
                ImGui::EndTooltip();

                if (HoveredOwnerInfo.Component && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    DetailsPanel.FocusComponentDetails(
                        HoveredOwnerInfo.Component,
                        HoveredOwnerInfo.CascadeIndex,
                        HoveredOwnerInfo.FaceIndex);
                }
            }

            ImGui::Text("Allocations: %d", static_cast<int32>(SliceAllocations.size()));
            if (InactivePaddingAllocationCount > 0)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("| Padded: %u", InactivePaddingAllocationCount);
            }
        }
        else
        {
            ImGui::Text("Allocations: 0");
        }

        ImGui::EndChild();

        if ((SliceIndex % 2u) == 0u && SliceIndex + 1u < ShadowAtlas::SliceCount)
        {
            ImGui::SameLine();
        }
    }

    ImGui::End();
}
