#include "Subsystem/EditorCameraSubsystem.h"

#include "Component/CameraComponent.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputManager.h"
#include "Object/ObjectFactory.h"
#include "Pawn/EditorCameraPawn.h"
#include "Scene/Scene.h"
#include "World/World.h"

FEditorCameraSubsystem::~FEditorCameraSubsystem()
{
	Shutdown();
}

bool FEditorCameraSubsystem::Initialize(UWorld* ActiveWorld, FInputManager* InInputManager, FEnhancedInputManager* InEnhancedInputManager)
{
	Shutdown();

	if (ActiveWorld == nullptr)
	{
		return false;
	}

	EditorPawn = FObjectFactory::ConstructObject<AEditorCameraPawn>(nullptr, "EditorCameraPawn");
	if (EditorPawn == nullptr)
	{
		return false;
	}

	ActiveWorld->SetActiveCameraComponent(EditorPawn->GetCameraComponent());
	ViewportController.Initialize(
		EditorPawn->GetCameraComponent(),
		InInputManager,
		InEnhancedInputManager);
	return true;
}

void FEditorCameraSubsystem::Shutdown()
{
	if (EditorPawn)
	{
		EditorPawn->Destroy();
		EditorPawn = nullptr;
	}

	ViewportController.Cleanup();
}

void FEditorCameraSubsystem::Tick(UWorld* ActiveWorld, UScene* ActiveScene, float DeltaTime)
{
	SyncActiveCamera(ActiveWorld, ActiveScene);
	ViewportController.Tick(DeltaTime);
}

FEditorViewportController* FEditorCameraSubsystem::GetViewportController()
{
	return &ViewportController;
}

void FEditorCameraSubsystem::SyncActiveCamera(UWorld* ActiveWorld, UScene* ActiveScene)
{
	if (EditorPawn == nullptr || ActiveWorld == nullptr || ActiveScene == nullptr || !ActiveScene->IsEditorScene())
	{
		return;
	}

	UCameraComponent* EditorCamera = EditorPawn->GetCameraComponent();
	if (ActiveWorld->GetActiveCameraComponent() != EditorCamera)
	{
		ActiveWorld->SetActiveCameraComponent(EditorCamera);
	}
}
