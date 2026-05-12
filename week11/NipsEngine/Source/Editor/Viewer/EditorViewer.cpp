#include "EditorViewer.h"
#include "EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Geometry/Ray.h"
#include "Component/GizmoComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/ResourceManager.h"
#include "GameFramework/PrimitiveActors.h"
#include "imgui.h"

void FEditorViewer::Init(
    FWindowsWindow* InWindow,
    UEditorEngine* InEditor,
    UWorld* InWorld,
    FSelectionManager* InSelectionManager)
{
    Window = InWindow;
    Editor = InEditor;

    Viewport.SetClient(&Client);

    Client.Initialize(InWindow, InEditor);
    Client.SetWorld(InWorld);
    Client.SetGizmo(InSelectionManager->GetGizmo());
    Client.SetSelectionManager(InSelectionManager);

    Client.SetViewport(&Viewport);
    Client.SetState(&Viewport.GetState());

    Client.SetViewportType(EEditorViewportType::EVT_Perspective);
    Client.CreateCamera();
    Client.ApplyCameraMode();

	// 임시
	FViewportRect Rect = { 0, 0, 300, 300 };
    Viewport.SetRect(Rect);

	// Build 는 Viewer 담당
	ADirectionalLightActor* DirectionalLight = InWorld->SpawnActor<ADirectionalLightActor>();
    if (DirectionalLight)
    {
        DirectionalLight->InitDefaultComponents();
        DirectionalLight->SetFName(FName("Directional Light"));
        DirectionalLight->SetActorLocation(FVector(0.0f, 0.0f, 13.0f));
        DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
    }

    AAmbientLightActor* AmbientLight = InWorld->SpawnActor<AAmbientLightActor>();
    if (AmbientLight)
    {
        AmbientLight->InitDefaultComponents();
        AmbientLight->SetFName(FName("Ambient Light"));
        AmbientLight->SetActorLocation(FVector(0.0f, 0.0f, 15.0f));
    }

    InWorld->SyncSpatialIndex();

    ViewTarget = InWorld->SpawnActor<ASkeletalMeshActor>();
    ViewTarget->InitDefaultComponents();
    ViewTarget->SetFName(FName("ViewerActor"));
    ViewTarget->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
}

void FEditorViewer::Shutdown()
{
    // UEditorEngine::Shutdown 흐름:
    //   CloseScene() (ViewTarget actor 포함 destroy) → ... → Viewer.Shutdown()
    // 이 시점에 ViewTarget raw 포인터는 dangling이므로 RemoveComponent를 호출하면 안 됨.
    // SocketPreview 컴포넌트들은 ViewTarget actor 소멸자가 이미 함께 destroy함.
    // 우리는 단순히 map만 비우고 포인터를 null로 잡는다.
    SocketPreviewMeshes.clear();
    ViewTarget = nullptr;

    Viewport.SetClient(nullptr);
}

void FEditorViewer::Tick(float DeltaTime)
{
    Client.Tick(DeltaTime);
}

void FEditorViewer::SetSocketPreviewMesh(const FName& SocketName, const FString& StaticMeshPath)
{
    if (!ViewTarget) return;

    // 같은 socket에 이미 preview가 있으면 먼저 제거 (덮어쓰기 동작)
    ClearSocketPreview(SocketName);

    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh(StaticMeshPath);
    if (!Mesh) return;

    UStaticMeshComponent* Preview = ViewTarget->AddComponent<UStaticMeshComponent>();
    if (!Preview) return;

    // 휘발성 + 에디터 전용. Scene 저장에 안 들어가고, 게임 빌드 render에 안 잡힘.
    Preview->SetTransient(true);
    Preview->SetEditorOnly(true);
    Preview->SetStaticMesh(Mesh);

    if (USkeletalMeshComponent* SkComp = ViewTarget->GetSkeletalMeshComponent())
    {
        Preview->AttachToComponent(SkComp, SocketName);
    }

    SocketPreviewMeshes[SocketName] = Preview;
}

void FEditorViewer::ClearSocketPreview(const FName& SocketName)
{
    auto It = SocketPreviewMeshes.find(SocketName);
    if (It == SocketPreviewMeshes.end()) return;

    if (ViewTarget && It->second)
    {
        ViewTarget->RemoveComponent(It->second);
        // RemoveComponent가 UObjectManager::DestroyObject까지 호출 — 추가 cleanup 불필요.
    }
    SocketPreviewMeshes.erase(It);
}

void FEditorViewer::ClearAllSocketPreviews()
{
    if (ViewTarget)
    {
        for (auto& [Name, Comp] : SocketPreviewMeshes)
        {
            if (Comp)
            {
                ViewTarget->RemoveComponent(Comp);
            }
        }
    }
    SocketPreviewMeshes.clear();
}

UStaticMeshComponent* FEditorViewer::FindPreviewMesh(const FName& SocketName) const
{
    auto It = SocketPreviewMeshes.find(SocketName);
    return It != SocketPreviewMeshes.end() ? It->second : nullptr;
}