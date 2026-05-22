#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"
#include "Render/Common/ComPtr.h"

class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
struct ID3D11ShaderResourceView;

class FAnimationEditorViewerWidget : public FEditorViewerWidget
{
public:
	~FAnimationEditorViewerWidget() override = default;

protected:
	void RenderContent(float DeltaTime) override;

private:
	void RenderAnimSequenceLeftPanel(UAnimSequence* Sequence, USkeletalMeshComponent* SkelMeshComp);
	void RenderAnimSequenceRightPanel(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh);
	void RenderAnimSequenceToolbar(UAnimSequence* Sequence);
	void RenderAnimSequenceTimeline(UAnimSequence* Sequence);
	void RenderAnimSequenceDetails(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh);
	bool SaveAnimSequenceAsset(UAnimSequence* Sequence);
	void RenderAnimSequenceList(UAnimSequence* Sequence);
	void SyncPreviewMeshPathBuffer();
	void LoadAnimSequenceToolbarIcons();
	bool DrawAnimSequenceIconButton(
		const char* Id,
		ID3D11ShaderResourceView* Icon,
		const char* Tooltip,
		const ImVec2& Size);

	FString PreviewMeshPathBufferSource;
	char PreviewMeshPathBuffer[1024] = {};
	int32 SelectedAnimTrackIndex = -1;
	int32 SelectedAnimNotifyIndex = -1;
	int32 DraggingAnimNotifyIndex = -1;
	int32 AnimNotifyDragMode = 0;
	float AnimNotifyDragGrabOffset = 0.0f;
	bool bAnimNotifyDragDirty = false;
	UAnimSequence* CachedAnimSequence = nullptr;
	float PendingAnimNotifyTimeToAdd = 0.0f;
	char SelectedAnimNotifyNameBuffer[128] = {};
	int32 SelectedAnimNotifyNameBufferIndex = -1;
	float AnimNotifyDurationToAdd = 0.0f;

	bool bAnimSequenceToolbarIconsLoadAttempted = false;
	TComPtr<ID3D11ShaderResourceView> AnimSequencePlayIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequencePauseIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceReverseIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceToFrontIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceToEndIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceLoopingIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceNoLoopingIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceToNextingIcon;
	TComPtr<ID3D11ShaderResourceView> AnimSequenceToPreviousingIcon;
};
