#pragma once

#include "Controller/EditorViewportController.h"
#include "Types/ObjectPtr.h"

class AEditorCameraPawn;
class FEnhancedInputManager;
class FInputManager;
class UScene;
class UWorld;

class FEditorCameraSubsystem
{
public:
	~FEditorCameraSubsystem();

	bool Initialize(UWorld* ActiveWorld, FInputManager* InInputManager, FEnhancedInputManager* InEnhancedInputManager);
	void Shutdown();
	void PrepareFrame(UWorld* ActiveWorld, UScene* ActiveScene, float DeltaTime);

	FEditorViewportController* GetViewportController();

private:
	void SyncActiveCamera(UWorld* ActiveWorld, UScene* ActiveScene);

	TObjectPtr<AEditorCameraPawn> EditorPawn;
	FEditorViewportController ViewportController;
};
