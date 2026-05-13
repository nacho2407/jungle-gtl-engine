#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Map.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"
#include "Object/FName.h"

class UEditorEngine;
class UWorld;
class FSelectionManager;
class FWindowsWindow;
struct ID3D11ShaderResourceView;
class ASkeletalMeshActor;
class UStaticMeshComponent;

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

	FSkeletalMeshViewportClient&       GetClient()       { return Client; }
	const FSkeletalMeshViewportClient& GetClient() const { return Client; }

	// 우선은 Skeletal Mesh Viewer 테스트용으로 추상화 안함
	ASkeletalMeshActor* GetViewTarget() const { return ViewTarget; }
    void ClearViewTarget() { ViewTarget = nullptr; }

    // Socket Preview Mesh API (Phase 4) — 휘발성. transient + editorOnly로
    // scene 저장과 게임 빌드 양쪽에서 자동으로 빠짐.
    void SetSocketPreviewMesh(const FName& SocketName, const FString& StaticMeshPath);
    void ClearSocketPreview(const FName& SocketName);
    void ClearAllSocketPreviews();
    UStaticMeshComponent* FindPreviewMesh(const FName& SocketName) const;

private:

private:
    FSceneViewport Viewport;
    FSkeletalMeshViewportClient Client;

    UEditorEngine* Editor = nullptr;
    FWindowsWindow* Window = nullptr;

	ASkeletalMeshActor* ViewTarget = nullptr;

    TMap<FName, UStaticMeshComponent*, FName::Hash> SocketPreviewMeshes;
};
