#pragma once

#include "GameFramework/World.h"
#include "Component/Camera.h"
#include "Component/GizmoComponent.h"

#include "Render/Renderer/Renderer.h"
#include "Render/Scene/RenderBus.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorMainPanel.h"

#include "Engine/Serialization/SceneSaveManager.h"
#include "Render/FontBatcher.h"

class FEditorEngine
{
private:
	struct FRuntimeSettings
	{
		bool bLimitUpdateRate = true;
		int UpdateRate = 60;
	};


	UWorld* EditorWorld = nullptr;
	UCamera* EditorCamera = nullptr;
	UGizmoComponent* EditorGizmo = nullptr;
	HWND HWindow = nullptr;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	// Initial Camera Dimensions
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	uint32 CurrentWorld = 0;
	TArray<UWorld*> Scene;

	FRenderHandler RenderHandler;

	FRenderer Renderer;
	FRenderBus RenderBus;
	FEditorMainPanel MainPanel;
	FEditorViewportClient ViewportClient;
	float MainLoopFPS = 0.0f;
	FRuntimeSettings RuntimeSettings;

	
private:
void UpdateWorld(float DeltaTime);
void SyncCameraFromRenderHandler();
void BuildRenderCommands();





public:
void Create(HWND InHWindow);
void Release();
void OnWindowResized(uint32 Width, uint32 Height);
void BeginPlay();
void BeginFrame(float DeltaTime);
void Update(float DeltaTime);
void Render(float DeltaTime);
void EndFrame();

UWorld* GetWorld() const { 
	//return EditorWorld;
	return Scene[CurrentWorld];
}
TArray<UWorld*>& GetScene() { return Scene; }
uint32 GetCurrentWorld() const { return CurrentWorld; }
void SetCurrentWorld(uint32 NewWorldIndex) { CurrentWorld = NewWorldIndex; }
UCamera* GetCamera() const { return EditorCamera; }
UGizmoComponent* GetGizmo() const { return EditorGizmo; }
FRenderHandler& GetRenderHandler() { return RenderHandler; }
FCameraState& GetCameraState() { return EditorCamera->GetCameraState(); }
const FCameraState& GetCameraState() const { return EditorCamera->GetCameraState(); }
void ResetCamera(UCamera* Camera);
void ClearScene();
void ResetViewport();
void CloseScene();
void NewScene();
	void SetMainLoopFPS(float InFPS) { MainLoopFPS = InFPS; }
	float GetMainLoopFPS() const { return MainLoopFPS; }

	bool IsUpdateRateLimited() const { return RuntimeSettings.bLimitUpdateRate; }
	void SetUpdateRateLimited(bool bLimited) { RuntimeSettings.bLimitUpdateRate = bLimited; }
	int GetUpdateRate() const { return RuntimeSettings.UpdateRate; }
	void SetUpdateRate(int NewRate) { RuntimeSettings.UpdateRate = (NewRate < 1) ? 1 : NewRate; }

// Legacy names for compatibility during migration
UWorld* GetEditorWorld() const { return GetWorld(); }
UCamera* GetEditorCamera() const { return GetCamera(); }
UGizmoComponent* GetEditorGizmo() const { return GetGizmo(); }
FCameraState& GetEditorCameraState() { return GetCameraState(); }
const FCameraState& GetEditorCameraState() const { return GetCameraState(); }

template <typename T>
AActor* SpawnNewPrimitiveActor(FVector InitLocation)
{
	AActor* NewActor = Scene[CurrentWorld]->SpawnActor<AActor>();
	NewActor->SetActorLocation(InitLocation);
	NewActor->AddComponent<T>();
	return NewActor;
}
};
