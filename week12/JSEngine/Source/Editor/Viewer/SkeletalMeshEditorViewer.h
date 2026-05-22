#pragma once

#include "Core/Containers/Map.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Object/FName.h"

class UStaticMeshComponent;

class FSkeletalMeshEditorViewer : public FEditorViewer
{
public:
    bool ChangeTarget(const FString& InFileName) override;
    EEditorTabKind GetTabKind() const override;
    const char* GetViewerLabel() const override;

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

    void SetSocketPreviewMesh(const FName& SocketName, const FString& StaticMeshPath);
    void ClearSocketPreview(const FName& SocketName);
    void ClearAllSocketPreviews();
    UStaticMeshComponent* FindPreviewMesh(const FName& SocketName) const;

protected:
    bool HandleViewportBonePick(float LocalX, float LocalY) override;

private:
    TMap<FName, UStaticMeshComponent*, FName::Hash> SocketPreviewMeshes;
    int32 SelectedBoneIndex = -1;
    int32 SelectedSocketIndex = -1;
    FVector CachedRotation;
};
