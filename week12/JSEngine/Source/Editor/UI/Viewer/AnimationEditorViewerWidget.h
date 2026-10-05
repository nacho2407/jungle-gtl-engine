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
	// Mesh & Anim Sequence
	void SyncPreviewMeshPathBuffer();
	bool SaveAnimSequenceAsset(UAnimSequence* Sequence);

	// Render Panels
	void RenderAnimSequenceLeftPanel(UAnimSequence* Sequence, USkeletalMeshComponent* SkelMeshComp);
	void RenderAnimSequenceRightPanel(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh);
	void RenderAnimSequenceToolbar(UAnimSequence* Sequence);
	void RenderAnimSequenceTimeline(UAnimSequence* Sequence);
	void RenderAnimSequenceDetails(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh);
	void RenderAnimSequenceList(UAnimSequence* Sequence);

	// Icons
	void LoadAnimSequenceToolbarIcons();
	bool DrawAnimSequenceIconButton(const char* Id, ID3D11ShaderResourceView* Icon, const char* Tooltip, const ImVec2& Size);

private:
	// Cached Data
	UAnimSequence* CachedAnimSequence = nullptr;

	FString PreviewMeshPathBufferSource;
	char PreviewMeshPathBuffer[1024] = {};

	// Animation Editing State (Track, Notify, Dragging)
	int32 SelectedAnimTrackIndex = -1;
	int32 SelectedAnimNotifyIndex = -1;
	int32 DraggingAnimNotifyIndex = -1;
	
	int32 AnimNotifyDragMode = 0;
	float AnimNotifyDragGrabOffset = 0.0f;
	bool bAnimNotifyDragDirty = false;
	
	float PendingAnimNotifyTimeToAdd = 0.0f;
	char SelectedAnimNotifyNameBuffer[128] = {};
	int32 SelectedAnimNotifyNameBufferIndex = -1;
	float AnimNotifyDurationToAdd = 0.0f;

private:
	// Icons
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
