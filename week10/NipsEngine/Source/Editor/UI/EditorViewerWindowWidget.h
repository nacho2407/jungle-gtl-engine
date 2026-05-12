#pragma once
#include "Editor/UI/EditorWidget.h"
#include "Asset/SkeletalMeshTypes.h"

class FEditorViewerWindowWidget : public FEditorWidget
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;
    void DrawBoneNode(
        class FEditorViewer& Viewer,
        int32 BoneIndex,
        const TArray<FBoneInfo>& Bones,
        const TArray<TArray<int32>>& Children);

private:
    void RenderBoneDetails(class FEditorViewer& Viewer, class USkeletalMeshComponent* SkelComp);

private:
    TArray<TArray<int32>> Children;
    FSkeletalMesh* CachedMesh = nullptr;
};
