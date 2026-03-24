#include "ControlPanel.h"

#include "Camera/ViewportCamera.h"
#include "Core/Math/MathUtility.h"
#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "Engine/EngineStatics.h"
#include "Viewport/EditorViewportClient.h"
#include "imgui.h"

namespace
{
    constexpr const char* ProjectionTypeLabels[] = {"Perspective", "Orthographic"};
    constexpr const char* ViewModeLabels[] = {"Lit", "Unlit", "Wireframe"};

    void DrawVectorRow(const char* Label, FVector& Value, float Speed = 0.1f)
    {
        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3("##Value", Value.XYZ, Speed);
        ImGui::PopID();
    }

    void DrawRotatorRow(const char* Label, FRotator& Value, float Speed = 0.5f)
    {
        FVector EulerDegrees = Value.Euler();

        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##Value", EulerDegrees.XYZ, Speed))
        {
            // Camera panel도 내부 Rotator 순서 대신 축 기반 X/Y/Z 회전으로 편집합니다.
            Value = FRotator::MakeFromEuler(EulerDegrees);
        }
        ImGui::PopID();
    }
} // namespace

const wchar_t* FControlPanel::GetPanelID() const
{
    return L"ControlPanel";
}

const wchar_t* FControlPanel::GetDisplayName() const
{
    return L"Control Panel";
}

void FControlPanel::Draw()
{
    if (!ImGui::Begin("Control Panel", nullptr))
    {
        ImGui::End();
        return;
    }

    FViewportCamera* Camera = ResolveViewportCamera();
    if (Camera == nullptr)
    {
        DrawUnavailableState();
        ImGui::End();
        return;
    }

    DrawTransformSection(*Camera);
    ImGui::Separator();
    DrawProjectionSection(*Camera);
    ImGui::Separator();
    DrawViewModeSection();
    ImGui::Separator();
    DrawNavigationSection();
    ImGui::Separator();
    DrawWorldSection();

    ImGui::End();
}

FViewportCamera* FControlPanel::ResolveViewportCamera() const
{
    if (GetContext() == nullptr || GetContext()->Editor == nullptr)
    {
        return nullptr;
    }

    return &GetContext()->Editor->GetViewportClient().GetCamera();
}

void FControlPanel::DrawUnavailableState() const
{
    ImGui::TextUnformatted("Editor camera is unavailable.");
    ImGui::Spacing();
    ImGui::TextWrapped("The control panel requires an active editor viewport client.");
}

void FControlPanel::DrawTransformSection(FViewportCamera& Camera) const
{
    ImGui::TextUnformatted("Camera Transform");

    FVector Location = Camera.GetLocation();
    FRotator Rotation = Camera.GetRotation().Rotator();

    DrawVectorRow("Location", Location, 0.1f);
    DrawRotatorRow("Rotation", Rotation, 0.5f);

    if (Camera.GetLocation() != Location)
    {
        Camera.SetLocation(Location);
    }

    if (!Camera.GetRotation().Rotator().Equals(Rotation))
    {
        Camera.SetRotation(Rotation);
    }
}

void FControlPanel::DrawProjectionSection(FViewportCamera& Camera) const
{
    ImGui::TextUnformatted("Projection");

    int CurrentProjection = static_cast<int>(Camera.GetProjectionType());
    if (ImGui::Combo("Projection Type", &CurrentProjection, ProjectionTypeLabels,
                     IM_ARRAYSIZE(ProjectionTypeLabels)))
    {
        Camera.SetProjectionType(static_cast<EViewportProjectionType>(CurrentProjection));
    }

    if (Camera.GetProjectionType() == EViewportProjectionType::Perspective)
    {
        float FOVDegrees = FMath::RadiansToDegrees(Camera.GetFOV());
        if (ImGui::SliderFloat("FOV", &FOVDegrees, 30.0f, 120.0f, "%.1f deg"))
        {
            Camera.SetFOV(FMath::DegreesToRadians(FOVDegrees));
        }
    }
    else
    {
        float OrthoHeight = Camera.GetOrthoHeight();
        if (ImGui::DragFloat("Ortho Height", &OrthoHeight, 0.1f, 1.0f, 1000.0f, "%.1f"))
        {
            Camera.SetOrthoHeight(FMath::Clamp(OrthoHeight, 1.0f, 1000.0f));
        }
    }

    ImGui::Spacing();
    ImGui::Text("Viewport: %u x %u", Camera.GetWidth(), Camera.GetHeight());
    ImGui::Text("Aspect Ratio: %.3f", Camera.GetAspectRatio());
}

void FControlPanel::DrawViewModeSection() const
{
    if (GetContext() == nullptr || GetContext()->Editor == nullptr)
    {
        return;
    }

    FViewportRenderSetting& RenderSetting =
        GetContext()->Editor->GetViewportClient().GetRenderSetting();

    ImGui::TextUnformatted("View Mode");

    int CurrentViewMode = static_cast<int>(RenderSetting.GetViewMode());
    if (ImGui::Combo("Shading", &CurrentViewMode, ViewModeLabels, IM_ARRAYSIZE(ViewModeLabels)))
    {
        RenderSetting.SetViewMode(static_cast<EViewModeIndex>(CurrentViewMode));
    }
}

void FControlPanel::DrawNavigationSection() const
{
    if (GetContext() == nullptr || GetContext()->Editor == nullptr)
    {
        return;
    }

    FViewportNavigationController& NavigationController =
        GetContext()->Editor->GetViewportClient().GetNavigationController();

    ImGui::TextUnformatted("Navigation");

    float MoveSpeed = NavigationController.GetMoveSpeed();
    if (ImGui::DragFloat("Move Speed", &MoveSpeed, 1.0f, 10.0f, 2000.0f, "%.1f"))
    {
        NavigationController.SetMoveSpeed(MoveSpeed);
    }

    float RotationSpeed = NavigationController.GetRotationSpeed();
    if (ImGui::DragFloat("Rotation Speed", &RotationSpeed, 0.01f, 0.01f, 10.0f, "%.2f"))
    {
        NavigationController.SetRotationSpeed(RotationSpeed);
    }
}

void FControlPanel::DrawWorldSection() const
{
    ImGui::TextUnformatted("World");

    float GridSpacing = UEngineStatics::GridSpacing;
    if (ImGui::DragFloat("Grid Spacing", &GridSpacing, 0.1f, 1.0f, 1000.0f, "%.1f"))
    {
        UEngineStatics::GridSpacing = FMath::Clamp(GridSpacing, 1.0f, 1000.0f);
    }
}
