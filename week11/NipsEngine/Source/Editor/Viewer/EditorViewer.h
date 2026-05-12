#pragma once

#include "Core/CoreTypes.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"

class UEditorEngine;
class UWorld;
class FSelectionManager;
class FWindowsWindow;
struct ID3D11ShaderResourceView;
class ASkeletalMeshActor;

class FEditorViewer
{
public:
    void Init(
        FWindowsWindow* InWindow,
        UEditorEngine* InEditor,
        UWorld* InWorld,
        FSelectionManager* InSelectionManager);

    void Shutdown();

    void Tick(float DeltaTime);

    void SetRect(const FViewportRect& InRect) 
    { 
        Viewport.SetRect(InRect); 
        Client.SetViewportSize((float)InRect.Width, (float)InRect.Height);
    }

	ID3D11ShaderResourceView* GetSRV() const
    {
        return Viewport.GetOutSRV();
    }

	FSceneViewport& GetViewport() { return Viewport; }

	// 우선은 Skeletal Mesh Viewer 테스트용으로 추상화 안함
	ASkeletalMeshActor* GetViewTarget() const { return ViewTarget; }

	void ChangeTarget(const FString& InFileName);

	int32 SelectedBoneIndex = -1;
	FVector CachedRotation;
    // Local holder for gizmo-selected actors when using bone proxy in the viewer.
    TArray<AActor*> GizmoSelectedActors;

private:
    FSceneViewport Viewport;
    FSkeletalMeshViewportClient Client;

    UEditorEngine* Editor = nullptr;
    FWindowsWindow* Window = nullptr;

	ASkeletalMeshActor* ViewTarget = nullptr;
};