#include "AnimationEditorViewer.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "GameFramework/PrimitiveActors.h"

#include <algorithm>

UAnimSingleNodeInstance* FAnimationEditorViewer::GetSingleNodeInstance() const
{
	if (!GetViewTarget())
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkelComp = GetViewTarget()->GetSkeletalMeshComponent();
	return SkelComp ? Cast<UAnimSingleNodeInstance>(SkelComp->GetAnimInstance()) : nullptr;
}

bool FAnimationEditorViewer::ApplyAnimationSequenceToComponent(bool bAutoPlay)
{
	if (!GetViewTarget() || !AnimSequence)
	{
		return false;
	}

	USkeletalMeshComponent* SkelComp = GetViewTarget()->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return false;
	}

	if (PreviewMeshPath.empty())
	{
		PreviewMeshPath = AnimSequence->GetPreviewMeshPath();
	}
	if (PreviewMeshPath.empty())
	{
		PreviewMeshPath = AnimSequence->GetSourceFilePath();
	}

	USkeletalMesh* PreviewMesh = PreviewMeshPath.empty()
		? nullptr
		: FResourceManager::Get().LoadSkeletalMesh(PreviewMeshPath);

	if (!PreviewMesh)
	{
		SkelComp->Stop();
		SkelComp->SetAnimation(nullptr);
		SkelComp->SetSkeletalMesh(nullptr);
		return false;
	}

	SkelComp->SetSkeletalMesh(PreviewMesh);
	SkelComp->PlayAnimation(AnimSequence, true);
	SkelComp->SetAnimationPosition(0.0f);
	if (!bAutoPlay)
	{
		SkelComp->Pause();
	}

	return true;
}

bool FAnimationEditorViewer::SetAnimationSequencePreviewMesh(const FString& InPreviewMeshPath)
{
	if (!AnimSequence)
	{
		return false;
	}

	PreviewMeshPath = InPreviewMeshPath;
	return ApplyAnimationSequenceToComponent(IsAnimationPlaying());
}

void FAnimationEditorViewer::RestartAnimation()
{
	SetAnimationTime(0.0f);
	SetAnimationPlaying(true);
}

void FAnimationEditorViewer::SetAnimationPlaying(bool bInPlaying)
{
	if (!GetViewTarget())
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = GetViewTarget()->GetSkeletalMeshComponent();
	if (!SkelComp || !AnimSequence)
	{
		return;
	}

	if (!GetSingleNodeInstance())
	{
		ApplyAnimationSequenceToComponent(false);
	}

	if (bInPlaying)
	{
		SkelComp->Play(IsAnimationLooping());
	}
	else
	{
		SkelComp->Pause();
	}
}

void FAnimationEditorViewer::SetAnimationLooping(bool bInLooping)
{
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		SingleNode->SetLooping(bInLooping);
	}
}

void FAnimationEditorViewer::SetAnimationPlayRate(float InPlayRate)
{
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		SingleNode->SetPlayRate(InPlayRate);
	}
}

void FAnimationEditorViewer::SetAnimationTime(float InTime)
{
	if (!GetSingleNodeInstance())
	{
		ApplyAnimationSequenceToComponent(false);
	}

	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		const float Length = SingleNode->GetLength();
		SingleNode->SetPosition(Length > 0.0f ? std::clamp(InTime, 0.0f, Length) : 0.0f);
	}
}

float FAnimationEditorViewer::GetAnimationCurrentTime() const
{
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		return SingleNode->GetCurrentTime();
	}
	return 0.0f;
}

float FAnimationEditorViewer::GetAnimationLength() const
{
	return AnimSequence ? AnimSequence->GetPlayLength() : 0.0f;
}

float FAnimationEditorViewer::GetAnimationPlayRate() const
{
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		return SingleNode->GetPlayRate();
	}
	return 1.0f;
}

bool FAnimationEditorViewer::IsAnimationPlaying() const
{
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		return SingleNode->IsPlaying();
	}
	return false;
}

bool FAnimationEditorViewer::IsAnimationLooping() const
{
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		return SingleNode->IsLooping();
	}
	return false;
}

bool FAnimationEditorViewer::ChangeTarget(const FString& InFileName)
{
	SetFileName(InFileName);
	ClearBaseSelection();
	AnimSequence = FResourceManager::Get().LoadAnimSequence(FPaths::Normalize(InFileName));
	PreviewMeshPath.clear();

	if (!AnimSequence)
	{
		return false;
	}

	PreviewMeshPath = AnimSequence->GetPreviewMeshPath();
	if (PreviewMeshPath.empty())
	{
		PreviewMeshPath = AnimSequence->GetSourceFilePath();
	}

	return ApplyAnimationSequenceToComponent(true);
}

EEditorTabKind FAnimationEditorViewer::GetTabKind() const
{
	return EEditorTabKind::AnimSequenceViewer;
}

const char* FAnimationEditorViewer::GetViewerLabel() const
{
	return "Animation Sequence Viewer";
}
