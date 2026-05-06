#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelPlacementHelpers.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Core/Paths.h"
#include "Core/ResourceManager.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Serialization/PrefabManager.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace
{
bool HasParentDirectoryReference(const std::filesystem::path& Path)
{
    for (const std::filesystem::path& Part : Path)
    {
        if (Part == L"..")
        {
            return true;
        }
    }
    return false;
}

std::wstring GetLowerExtension(const std::filesystem::path& Path)
{
    std::wstring Extension = Path.extension().wstring();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
    return Extension;
}

FString ResolveStaticMeshDropLoadPath(const FString& PayloadPath)
{
    std::filesystem::path Path(FPaths::ToWide(PayloadPath));
    const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
    if (!Path.is_absolute())
    {
        Path = RootPath / Path;
    }
    Path = Path.lexically_normal();

    const std::filesystem::path RelativePath = Path.lexically_relative(RootPath);
    if (RelativePath.empty() || HasParentDirectoryReference(RelativePath))
    {
        return {};
    }

    const std::wstring Extension = GetLowerExtension(Path);
    if (Extension == L".obj")
    {
        return FPaths::Normalize(FPaths::ToUtf8(RelativePath.generic_wstring()));
    }
    if (Extension == L".bin")
    {
        return FPaths::Normalize(FPaths::ToUtf8(Path.generic_wstring()));
    }
    return {};
}
}

bool FEditorMainPanel::SpawnStaticMeshFromContentPath(
    const FString& PayloadPath,
    int32 ViewportIndex,
    float LocalX,
    float LocalY
)
{
    if (!EditorEngine)
    {
        return false;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
    if (!Client || !Client->AllowsEditorWorldControl())
    {
        return false;
    }

    const FString MeshLoadPath = ResolveStaticMeshDropLoadPath(PayloadPath);
    if (MeshLoadPath.empty())
    {
        PushFooterLog("Unsupported static mesh drop");
        return false;
    }

    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh(MeshLoadPath);
    if (!Mesh || !Mesh->HasValidMeshData())
    {
        PushFooterLog("Failed to load dropped static mesh");
        return false;
    }

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World)
    {
        return false;
    }

    EditorEngine->GetUndoSystem().CaptureSnapshot("Place Static Mesh");
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
    if (!Actor)
    {
        return false;
    }

    Actor->InitDefaultComponents();
    Actor->SetActorLocation(FEditorMainPanelPlacementHelpers::ComputePlacementLocation(Client, LocalX, LocalY));
    if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Actor->GetRootComponent()))
    {
        StaticMeshComp->SetStaticMesh(Mesh);
    }

    Layout.SetLastFocusedViewportIndex(ViewportIndex);
    EditorEngine->GetSelectionManager().Select(Actor);
    Widgets.SceneWidget.MarkSceneDirty();
    PushFooterLog("StaticMesh actor placed from Content Browser");
    return true;
}

bool FEditorMainPanel::SpawnPrefabFromContentPath(
    const FString& PayloadPath,
    int32 ViewportIndex,
    float LocalX,
    float LocalY
)
{
    if (!EditorEngine)
    {
        return false;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
    if (!Client || !Client->AllowsEditorWorldControl())
    {
        return false;
    }

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World)
    {
        return false;
    }

    std::filesystem::path PrefabPath;
    std::error_code PrefabEc;
    if (!FPrefabManager::ResolvePrefabPath(PayloadPath, PrefabPath, false) ||
        !std::filesystem::is_regular_file(PrefabPath, PrefabEc))
    {
        PushFooterLog("Failed to find prefab");
        return false;
    }

    EditorEngine->GetUndoSystem().CaptureSnapshot("Place Prefab");
    AActor* Actor = FPrefabManager::SpawnActorFromPrefab(World, PayloadPath);
    if (!Actor)
    {
        PushFooterLog("Failed to spawn prefab");
        return false;
    }

    Actor->SetActorLocation(FEditorMainPanelPlacementHelpers::ComputePlacementLocation(Client, LocalX, LocalY));
    World->SyncSpatialIndex();
    Layout.SetLastFocusedViewportIndex(ViewportIndex);
    EditorEngine->GetSelectionManager().Select(Actor);
    Widgets.SceneWidget.MarkSceneDirty();
    PushFooterLog("Prefab actor placed from Content Browser");
    return true;
}

bool FEditorMainPanel::SpawnPrefabAtOrigin(const FString& PayloadPath)
{
    if (!EditorEngine)
    {
        return false;
    }

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World)
    {
        return false;
    }

    std::filesystem::path PrefabPath;
    std::error_code PrefabEc;
    if (!FPrefabManager::ResolvePrefabPath(PayloadPath, PrefabPath, false) ||
        !std::filesystem::is_regular_file(PrefabPath, PrefabEc))
    {
        PushFooterLog("Failed to find prefab");
        return false;
    }

    EditorEngine->GetUndoSystem().CaptureSnapshot("Place Prefab");
    AActor* Actor = FPrefabManager::SpawnActorFromPrefab(World, PayloadPath);
    if (!Actor)
    {
        PushFooterLog("Failed to spawn prefab");
        return false;
    }

    Actor->SetActorLocation(FVector::ZeroVector);
    World->SyncSpatialIndex();
    EditorEngine->GetSelectionManager().Select(Actor);
    Widgets.SceneWidget.MarkSceneDirty();
    PushFooterLog("Prefab actor placed at origin");
    return true;
}

void FEditorMainPanel::HandleContentBrowserViewportDrop()
{
    FString PayloadType;
    FString PayloadPath;
    if (!Widgets.ContentBrowserWidget.ConsumeReleasedDragPayload(PayloadType, PayloadPath))
    {
        return;
    }
    if ((PayloadType != "ObjectContentItem" && PayloadType != "PrefabContentItem") ||
        Widgets.ContentBrowserWidget.IsMouseOverBrowser())
    {
        return;
    }

    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 FocusedViewportIndex = Layout.GetLastFocusedViewportIndex();
    auto TryDropOnViewport = [&](int32 ViewportIndex) -> bool
    {
        FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
        if (!Client || !Client->AllowsEditorWorldControl())
        {
            return false;
        }

        const FViewportRect& Rect = Layout.GetSceneViewport(ViewportIndex).GetRect();
        if (Rect.Width <= 0 || Rect.Height <= 0)
        {
            return false;
        }
        if (MousePos.x < static_cast<float>(Rect.X) ||
            MousePos.x >= static_cast<float>(Rect.X + Rect.Width) ||
            MousePos.y < static_cast<float>(Rect.Y) ||
            MousePos.y >= static_cast<float>(Rect.Y + Rect.Height))
        {
            return false;
        }

        const float LocalX = MathUtil::Clamp(
            MousePos.x - static_cast<float>(Rect.X),
            0.0f,
            std::max(0.0f, static_cast<float>(Rect.Width - 1))
        );
        const float LocalY = MathUtil::Clamp(
            MousePos.y - static_cast<float>(Rect.Y),
            0.0f,
            std::max(0.0f, static_cast<float>(Rect.Height - 1))
        );
        if (PayloadType == "PrefabContentItem")
        {
            return SpawnPrefabFromContentPath(PayloadPath, ViewportIndex, LocalX, LocalY);
        }
        return SpawnStaticMeshFromContentPath(PayloadPath, ViewportIndex, LocalX, LocalY);
    };

    if (FocusedViewportIndex >= 0 &&
        FocusedViewportIndex < FEditorViewportLayout::MaxViewports &&
        TryDropOnViewport(FocusedViewportIndex))
    {
        return;
    }
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        if (i != FocusedViewportIndex && TryDropOnViewport(i))
        {
            return;
        }
    }
}
