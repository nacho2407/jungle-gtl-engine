#include "FpsOverlayWindow.h"

#include "EditorEngine.h"
#include "Core/Timer.h"
#include "Renderer/Renderer.h"
#include "Viewport/ViewportTypes.h"

#include "imgui.h"

#include <cstdio>

void FFpsOverlayWindow::Render(FEditorEngine* Engine, const FRect& AreaRect)
{
    if (!Engine)
    {
        return;
    }

	const FTimer& Timer = Engine->GetTimer();
	const float FPS = Timer.GetDisplayFPS();
	const float FrameTimeMs = Timer.GetFrameTimeMs();
	const FRenderInstrumentationStats& RenderStats = Engine->GetRenderInstrumentationStats();

	uint32 DrawCallCount = 0;
	uint32 StaticMeshDrawCallCount = RenderStats.StaticMeshDrawCallCount;
	if (FRenderer* Renderer = Engine->GetRenderer())
	{
		DrawCallCount = Renderer->GetFrameDrawCallCount();
		StaticMeshDrawCallCount = Renderer->GetFrameStaticMeshDrawCallCount();
	}

    const double LastPickTime = Engine->LastPickTime;
    const uint16 TotalPickCount = Engine->TotalPickCount;
    const double TotalPickTime = Engine->TotalPickTime;

    ImGuiViewport* MainVp = ImGui::GetMainViewport();
    const float PosX = MainVp->Pos.x + static_cast<float>(AreaRect.X) + 10.0f;
    const float PosY = MainVp->Pos.y + static_cast<float>(AreaRect.Y) + 10.0f;

    ImGui::SetNextWindowPos(ImVec2(PosX, PosY), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.02f, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));

    ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize;

    const bool bOpen = ImGui::Begin("##FpsOverlay", nullptr, Flags);

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);

    if (!bOpen)
    {
        ImGui::End();
        return;
    }

    // FPS 색상: 60 이상 초록, 30~60 노랑, 30 미만 빨강
    ImVec4 FpsColor;
    if (FPS >= 60.0f)
        FpsColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    else if (FPS >= 30.0f)
        FpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    else
        FpsColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

    ImGui::TextColored(FpsColor, "FPS: %.1f", FPS);
    ImGui::SameLine();
    ImGui::TextDisabled("(%.3f ms)", FrameTimeMs);

    ImGui::Separator();
    ImGui::Text("r.GpuOcclusionCulling : %s", RenderStats.bGpuOcclusionCullingEnabled ? "ON" : "OFF");
    ImGui::Text("Static Mesh Candidates : %u", RenderStats.StaticMeshCandidateCount);
    ImGui::Text("Static Mesh Frustum Passed : %u", RenderStats.FrustumPassedStaticMeshCount);
    ImGui::Text("Static Mesh Draw Calls : %u / %u",
        StaticMeshDrawCallCount,
        RenderStats.StaticMeshDrawCallCountBeforeOcclusion);
    ImGui::Text("Static Mesh Draw Skipped : %u | Skip Applied: %s",
        RenderStats.StaticMeshDrawSkippedCount,
        RenderStats.bOcclusionSkipApplied ? "Yes" : "No");
    ImGui::Text("Skipped Before BuildDrawCommands : %u | Late Draw Skip : %u",
        RenderStats.StaticMeshSkippedBeforeBuildDrawCommandsCount,
        RenderStats.StaticMeshSkippedLateDrawCount);
    ImGui::Text("Total Draw Calls : %u", DrawCallCount);
    ImGui::Text("Build Commands CPU : %.3f ms", static_cast<float>(RenderStats.ViewportBuildCommandsCpuMs));
    ImGui::Text("Collect Commands CPU : %.3f ms", static_cast<float>(RenderStats.CollectRenderCommandsCpuMs));
    ImGui::Text("Render Build CPU : %.3f ms", static_cast<float>(RenderStats.BuildRenderFrameCpuMs));
    ImGui::Text("Execute Commands CPU : %.3f ms", static_cast<float>(RenderStats.ExecuteRenderCommandsCpuMs));
    ImGui::Text("HZB Build : %s | Mips: %u | %.3f ms",
        RenderStats.bHZBBuildSucceeded ? "OK" : "Idle",
        RenderStats.HZBMipCount,
        static_cast<float>(RenderStats.HZBBuildCpuMs));
    ImGui::Text("Occlusion Pass : %s | Candidates: %u | %.3f ms",
        RenderStats.bOcclusionPassDispatched ? "Dispatched" : "Idle",
        RenderStats.OcclusionCandidateCount,
        static_cast<float>(RenderStats.OcclusionDispatchCpuMs));
    ImGui::Text("Occlusion Readback : %s / %s | Candidates: %u",
        RenderStats.bOcclusionReadbackIssued ? "Issued" : "Idle",
        RenderStats.bOcclusionReadbackCompleted ? "Completed" : "Pending",
        RenderStats.OcclusionReadbackCandidateCount);
    ImGui::Text("Occlusion Result : Visible %u | Occluded %u | Snapshot Match: %s",
        RenderStats.OcclusionVisibleCount,
        RenderStats.OcclusionOccludedCount,
        RenderStats.bOcclusionSnapshotReadbackMatched ? "OK" : "No");
    ImGui::Text("Picking    : %.3f ms | Count: %u | Total: %.3f ms",
        static_cast<float>(LastPickTime),
        static_cast<uint32>(TotalPickCount),
        static_cast<float>(TotalPickTime));

    ImGui::End();
}
