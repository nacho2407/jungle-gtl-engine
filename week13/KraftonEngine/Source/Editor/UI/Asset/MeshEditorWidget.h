#pragma once
#include "AssetEditorWidget.h"
#include "Editor/Viewport/MeshEditorViewportClient.h"
#include "Editor/UI/FbxImportOptionsDialog.h"
#include "Asset/AssetRegistry.h"

struct FSkeletalMesh;
struct ImDrawList;
struct ImVec2;
class UAnimSequence;
class UAnimMontage;
class UAnimSingleNodeInstance;

enum class EMeshEditorTab : uint8 { Skeleton, Mesh, Animation };

struct FAnimationTabState
{
	UAnimSequence* CurrentSequence    = nullptr;
	UAnimMontage*  CurrentMontage     = nullptr;
	int32          SelectedAnimIndex     = -1;
	int32          SelectedMontageIndex  = -1;
	bool           bMontageSelected      = false;     // true 면 좌측 패널이 montage 표시
	// 타임라인에서 선택된 Notify entry 인덱스 (현재 시퀀스의 DataModel->Notifies 기준).
	// -1 = 미선택. 시퀀스/몽타주 전환 시 -1 reset 필요.
	// 유효 시 좌상단 AssetDetails 패널이 시퀀스 정보 대신 Notify 의 UPROPERTY 편집 UI 를 그림.
	int32         SelectedNotifyIndex     = -1;
	int32         SelectedMorphCurveIndex = -1;
	int32         SelectedMorphKeyIndex   = -1;
	TArray<float> MorphPreviewWeights;
	TArray<uint8> MorphPreviewOverrideMask;
	bool          bMorphPreviewOverrideEnabled = false;

	// Animation tab asset browser cache.
	// Render 중 매 프레임 ListAnimationsForSkeleton() -> LoadAnimation() -> Serialize() 되는 것을 막는다.
	TArray<FAssetListItem> CachedAnimationFiles;
	FSkeletonBinding       CachedAnimationListBinding;
	bool                   bAnimationListDirty = true;

	float         AnimListWidth                = 200.0f;
	float         AnimDetailsWidth             = 280.0f;

	FFbxAnimationImportDialogState AnimationImportDialog;
};

class FMeshEditorWidget : public FAssetEditorWidget
{
public:
	FMeshEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

	void Render(float DeltaTime) override;

	bool IsMouseOverViewport() const { return IsOpen() && ViewportClient.IsMouseOverViewport(); }

	FMeshEditorViewportClient* GetViewportClient() { return &ViewportClient; }

	static void RecordImportDurationForAsset(const FString& AssetPath, double Seconds);
	static void ClearImportDurationForAsset(const FString& AssetPath);

private:
	// Tab bar
	void RenderTabBar();

	// Per-tab layouts
	void RenderSkeletonLayout();
	void RenderMeshLayout();
	void RenderAnimationLayout(float TotalHeight);

	// Shared helpers
	void RenderViewportPanel(ImVec2 Size);
	void RenderBoneTree(const FSkeletalMesh* Asset, int32 Index);
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;

	// Animation tab helpers
	void ApplyAnimationToComponent();
	void ResetMorphPreviewOverrides();
	void EnsureMorphPreviewOverrideSize();
	void ApplyMorphPreviewOverrides(TArray<float>& InOutMorphWeights) const;
	void RefreshAnimationPreviewPose();
	void MarkAnimationListDirty();
	const TArray<FAssetListItem>& GetCachedAnimationFilesForCurrentSkeleton();

private:
	FMeshEditorViewportClient ViewportClient;

	// Tab state
	EMeshEditorTab     ActiveTab = EMeshEditorTab::Skeleton;
	FAnimationTabState AnimTabState;

	// Skeleton tab state
	int32 SelectedBoneIndex = -1;
	float HierarchyWidth    = 250.0f;
	float DetailsWidth      = 300.0f;

	uint32  InstanceId;
	FName   PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;

	bool bPendingClose = false;
};
