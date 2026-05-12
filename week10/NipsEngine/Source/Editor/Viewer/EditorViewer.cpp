#include "EditorViewer.h"
#include "EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Geometry/Ray.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/ResourceManager.h"
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
    Viewport.SetClient(nullptr);
}

void FEditorViewer::Tick(float DeltaTime)
{
    Client.Tick(DeltaTime);
}

void FEditorViewer::ChangeTarget(const FString& InFileName)
{
    USkeletalMesh* SkelMesh = FResourceManager::Get().LoadSkeletalMesh(InFileName.c_str());
    if (SkelMesh)
	    ViewTarget->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
}
