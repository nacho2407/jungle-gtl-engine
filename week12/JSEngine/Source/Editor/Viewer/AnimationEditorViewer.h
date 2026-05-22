#pragma once

#include "Editor/Viewer/EditorViewer.h"

class UAnimSequence;
class UAnimSingleNodeInstance;

class FAnimationEditorViewer : public FEditorViewer
{
public:
	bool ChangeTarget(const FString& InFileName) override;
	EEditorTabKind GetTabKind() const override;
	const char* GetViewerLabel() const override;

	// Getter & Setter
	UAnimSequence* GetAnimSequence() const { return AnimSequence; }
	const FString& GetPreviewMeshPath() const { return PreviewMeshPath; }

	// Animation Setup
	bool SetAnimationSequencePreviewMesh(const FString& InPreviewMeshPath);

	// Animation Playback Controls
	void RestartAnimation();
	void SetAnimationPlaying(bool bInPlaying);
	void SetAnimationLooping(bool bInLooping);
	void SetAnimationPlayRate(float InPlayRate);
	void SetAnimationTime(float InTime);

	// Animation Playback Status
	float GetAnimationCurrentTime() const;
	float GetAnimationLength() const;
	float GetAnimationPlayRate() const;
	bool IsAnimationPlaying() const;
	bool IsAnimationLooping() const;

private:
	UAnimSingleNodeInstance* GetSingleNodeInstance() const;
	bool ApplyAnimationSequenceToComponent(bool bAutoPlay);

	UAnimSequence* AnimSequence = nullptr;
	FString PreviewMeshPath;
};
