#include "EditorViewer.h"

#include "EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Component/GizmoComponent.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "GameFramework/PrimitiveActors.h"

void FEditorViewer::Init(
	FWindowsWindow* InWindow,
	UEditorEngine* InEditor,
	UWorld* InWorld,
	FSelectionManager* InSelectionManager)
{
	FEditorViewportClient& Client = GetClient();

	Viewport.SetClient(&Client);

	Client.Initialize(InWindow, InEditor);
	Client.SetWorld(InWorld);
	Client.SetGizmo(InSelectionManager->GetGizmo());
	Client.SetSelectionManager(InSelectionManager);
	Client.SetSceneEditingShortcutsEnabled(false);

	Client.SetViewport(&Viewport);
	Client.SetState(&Viewport.GetState());
	Client.SetViewportType(EEditorViewportType::EVT_Perspective);
	Client.CreateCamera();
	Client.ApplyCameraMode();
	Viewport.GetState().ViewMode = EViewMode::Lit_BlinnPhong;
	Viewport.GetState().LightCullMode = ELightCullMode::None;

	const FViewportRect Rect = { 0, 0, 300, 300 };
	Viewport.SetRect(Rect);

	ADirectionalLightActor* DirectionalLight = InWorld->SpawnActor<ADirectionalLightActor>();
	if (DirectionalLight)
	{
		DirectionalLight->InitDefaultComponents();
		DirectionalLight->SetFName(FName("Viewer Directional Light"));
		DirectionalLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
		DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
	}

	AAmbientLightActor* AmbientLight = InWorld->SpawnActor<AAmbientLightActor>();
	if (AmbientLight)
	{
		AmbientLight->InitDefaultComponents();
		AmbientLight->SetFName(FName("Viewer Ambient Light"));
		AmbientLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));

		if (UAmbientLightComponent* AmbientLightComponent = AmbientLight->FindComponent<UAmbientLightComponent>())
		{
			AmbientLightComponent->Intensity = 0.7f;
		}
	}

	InWorld->SyncSpatialIndex();
}

void FEditorViewer::Shutdown()
{
	// UEditorEngine::Shutdown can destroy the preview world before viewer shutdown.
	// Keep this base cleanup limited to pointers owned by the viewport wrapper.
	Viewport.SetClient(nullptr);
}

void FEditorViewer::Tick(float DeltaTime)
{
	GetClient().Tick(DeltaTime);
}

void FEditorViewer::SetRect(const FViewportRect& InRect)
{
	Viewport.SetRect(InRect);
	GetClient().SetViewportSize(static_cast<float>(InRect.Width), static_cast<float>(InRect.Height));
}

void FEditorViewer::ClearBaseSelection()
{
	FEditorViewportClient& Client = GetClient();

	if (FSelectionManager* SelectionManager = Client.GetSelectionManager())
	{
		SelectionManager->ClearSelection();
		return;
	}

	if (UGizmoComponent* Gizmo = Client.GetGizmo())
	{
		Gizmo->Deactivate();
	}
}
