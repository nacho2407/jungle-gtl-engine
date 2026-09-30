// 에디터 영역의 세부 동작을 구현합니다.
#include "EditorMaterialPanel.h"
#include "Materials/Material.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"

#include "Component/PrimitiveComponent.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectIterator.h"
#include "Texture/Texture2D.h"
#include <algorithm>
#include <filesystem>

#include "ImGui/imgui.h"

namespace
{
}

#define MAT_SEPARATOR() \
    ImGui::Spacing();   \
    ImGui::Separator(); \
    ImGui::Spacing();

void FEditorMaterialPanel::Render(float DeltaTime)
{
    ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);
    ImGui::Begin("Material Editor");

    ImGui::End();
}

void FEditorMaterialPanel::ResetSelection()
{
    SelectedComponent = nullptr;
    SelectedSectionIndex = -1;
    SelectedMaterialPtr = nullptr;
}
