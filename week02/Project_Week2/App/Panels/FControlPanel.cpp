#include "FControlPanel.h"

#include "Core.h"
#include "FApplication.h"
#include "Component/UCameraComponent.h"
#include "GUI.h"

#include <cstring>

void FControlPanel::Render(FApplication* App)
{
    if (!App)
    {
        return;
    }

    UCameraComponent* Camera = App->GetMainCamera();
    if (!Camera)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(15.0f, 200.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 250.0f), ImGuiCond_Once);

    ImGui::Begin("Jungle Control Panel");

    ImGui::Text("Hello Jungle World!");

    // FPS 표시
    const uint32 FPS = static_cast<uint32>(ImGui::GetIO().Framerate); // 최근 프레임들의 평균 FPS 반환
    const uint32 FrameTimeMs = static_cast<uint32>((FPS > 0.0f) ? (1000.0f / FPS) : 0.0f); // FPS가 0인 경우 방어
    ImGui::Text("FPS %d (%d ms)", FPS, FrameTimeMs);

    bool bUseVSync = App->IsVSyncEnabled();
    if (ImGui::Checkbox("VSync", &bUseVSync))
    {
        App->SetVSyncEnabled(bUseVSync);
    }

    ImGui::Separator();

    const char* MeshItems[] = { "Sphere", "Cube", "Torus" };
    int CurrentMeshIndex = static_cast<int>(App->SelectedSpawnMeshType);

    if (ImGui::Combo("Mesh Type", &CurrentMeshIndex, MeshItems, IM_ARRAYSIZE(MeshItems)))
    {
        App->SelectedSpawnMeshType = static_cast<ESpawnMeshType>(CurrentMeshIndex);
    }

    if (ImGui::Button("Spawn"))
    {
        App->SpawnSelectedMeshActor();
    }
	ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputInt("Number of Spawn", &App->NumberOfSpawn);

    if (App->NumberOfSpawn < 1)
    {
        App->NumberOfSpawn = 1;
    }

    ImGui::Separator();

    static char SceneFileNameBuffer[256] = {};
    static FString LastSceneFileName;

    if (LastSceneFileName != App->GetSceneFileNameInput())
    {
        LastSceneFileName = App->GetSceneFileNameInput();
        std::memset(SceneFileNameBuffer, 0, sizeof(SceneFileNameBuffer));
        strncpy_s(SceneFileNameBuffer, LastSceneFileName.c_str(), _TRUNCATE);
    }

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputText("Scene Name", SceneFileNameBuffer, IM_ARRAYSIZE(SceneFileNameBuffer)))
    {
        App->SetSceneFileNameInput(SceneFileNameBuffer);
        LastSceneFileName = App->GetSceneFileNameInput();
    }

    if (ImGui::Button("New scene"))
    {
        App->NewScene();
    }

    if (ImGui::Button("Save scene"))
    {
        App->SaveScene();
    }

    if (ImGui::Button("Load scene"))
    {
        App->LoadScene();
    }

    ImGui::Separator();

    bool bProjectionChanged = false;

    if (ImGui::Checkbox("Orthogonal Projection", &App->bUseOrthogonalProjection))
    {
        bProjectionChanged = true;
    }

    if (&App->bUseOrthogonalProjection)
    {
        if (ImGui::SliderFloat("Ortho Width", &App->DebugOrthoWidth, 1.0f, 100.0f))
        {
            bProjectionChanged = true;
        }
    }

    if (bProjectionChanged)
    {
        App->ApplyCameraProjectionMode();
    }

    ImGui::Separator();

    float FOV = Camera->GetFieldOfView();
    FVector Location = Camera->GetRelativeLocation();
    FVector Rotation = Camera->GetRelativeRotation();

    bool bChanged = false;

    bChanged |= DrawFloatControl("FOV", FOV, 0.1f);
    bChanged |= DrawFloat3Control("Camera Location", Location, 0.1f);
    bChanged |= DrawFloat3Control("Camera Rotation", Rotation, 0.1f);
    

    if (FOV < 1.0f)
    {
        FOV = 1.0f;
    }
    else if (FOV > 179.0f)
    {
        FOV = 179.0f;
    }

    if (bChanged)
    {
        Camera->SetFieldOfView(FOV);
        Camera->SetRelativeLocation(Location);
        Camera->SetRelativeRotation(Rotation);
    }

    ImGui::End();
}

bool FControlPanel::DrawFloat3Control(const char* Label, FVector& Value, float Speed)
{
    bool bChanged = false;

	const float ItemWidth = 80.0f;

    ImGui::PushID(Label);

    ImGui::SetNextItemWidth(ItemWidth);
    bChanged |= ImGui::DragFloat("##X", &Value.X, Speed);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ItemWidth);
    bChanged |= ImGui::DragFloat("##Y", &Value.Y, Speed);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ItemWidth);
    bChanged |= ImGui::DragFloat("##Z", &Value.Z, Speed);

    ImGui::SameLine();
    ImGui::Text("%s", Label);

    ImGui::PopID();
    return bChanged;
}

bool FControlPanel::DrawFloatControl(const char* Label, float& Value, float Speed)
{
    bool bChanged = false;

	const float ItemWidth = 80.0f;
	const float TotalWidth = ItemWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;

    ImGui::PushID(Label);

	ImGui::SetNextItemWidth(TotalWidth);
    bChanged |= ImGui::DragFloat("##Value", &Value, Speed);

    ImGui::SameLine();
    ImGui::Text("%s", Label);

    ImGui::PopID();
    return bChanged;
}
