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

	ASkeletalMeshActor* GetViewTarget() const { return ViewTarget; }
    void ClearViewTarget() { ViewTarget = nullptr; }

    // Socket Preview Mesh API (Phase 4) — 휘발성. transient + editorOnly로
    // scene 저장과 게임 빌드 양쪽에서 자동으로 빠짐.
    void SetSocketPreviewMesh(const FName& SocketName, const FString& StaticMeshPath);
    void ClearSocketPreview(const FName& SocketName);
    void ClearAllSocketPreviews();
    UStaticMeshComponent* FindPreviewMesh(const FName& SocketName) const;

	void ChangeTarget(const FString& InFileName);

    const FString& GetFileName() const { return FileName; }
    void SelectBone(int32 BoneIndex);
    void SelectSocket(int32 SocketIndex);
    void ClearSelection();
    void NotifySocketDeleted(int32 SocketIndex);
    bool HandleBonePick(float LocalX, float LocalY);
    bool TryPickBone(float LocalX, float LocalY, int32& OutBoneIndex) const;

    int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
    int32 GetSelectedSocketIndex() const { return SelectedSocketIndex; }
    FVector& GetCachedBoneRotation() { return CachedRotation; }
    const FVector& GetCachedBoneRotation() const { return CachedRotation; }

private:
    FSceneViewport Viewport;
    FSkeletalMeshViewportClient Client;
	ASkeletalMeshActor* ViewTarget = nullptr;

    FString FileName;

    TMap<FName, UStaticMeshComponent*, FName::Hash> SocketPreviewMeshes;

    int32 SelectedBoneIndex = -1;
    int32 SelectedSocketIndex = -1;
    FVector CachedRotation;
};
