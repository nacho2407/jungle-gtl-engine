#pragma once
#include "Editor/UI/EditorWidget.h"
#include "Asset/SkeletalMeshTypes.h"

class FEditorViewerWindowWidget : public FEditorWidget
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;
    void DrawBoneNode(
        int32 BoneIndex,
        const TArray<FBoneInfo>& Bones,
        const TArray<TArray<int32>>& Children);

private:
    TArray<TArray<int32>> Children;
    FSkeletalMesh* CachedMesh = nullptr;
    int32 SelectedBoneIndex = -1;
};
