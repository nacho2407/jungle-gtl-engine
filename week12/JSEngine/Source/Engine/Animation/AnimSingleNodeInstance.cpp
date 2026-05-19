#include "AnimSingleNodeInstance.h"

#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <cmath>
#include <cstring>

namespace
{
	FString GetPersistentAnimationAssetPath(UAnimSequenceBase* Animation)
	{
		UAnimSequence* Sequence = Cast<UAnimSequence>(Animation);
		if (!Sequence)
		{
			return "";
		}

		if (!Sequence->GetAssetPath().empty())
		{
			return FPaths::Normalize(Sequence->GetAssetPath());
		}

		if (!Sequence->GetSourceFilePath().empty())
		{
			return FPaths::Normalize(Sequence->GetSourceFilePath());
		}

		return "";
	}
}

void UAnimSingleNodeInstance::Serialize(FArchive& Ar)
{
	UAnimInstance::Serialize(Ar);

	if (Ar.IsLoading())
	{
		ApplyAnimationFromAssetPath();
		SetPlayRate(PlayRate);
		SetLooping(bLooping);
		if (bAutoPlay && CurrentAnimation)
		{
			Play(bLooping);
		}
	}
}

void UAnimSingleNodeInstance::PostEditProperty(const char* PropertyName)
{
	UAnimInstance::PostEditProperty(PropertyName);

	if (PropertyName && std::strcmp(PropertyName, "AnimationAssetPath") == 0)
	{
		ApplyAnimationFromAssetPath();
	}
	else if (PropertyName && std::strcmp(PropertyName, "PlayRate") == 0)
	{
		SetPlayRate(PlayRate);
	}
	else if (PropertyName && std::strcmp(PropertyName, "bLooping") == 0)
	{
		SetLooping(bLooping);
	}
	else if (PropertyName && std::strcmp(PropertyName, "bAutoPlay") == 0)
	{
		if (bAutoPlay && CurrentAnimation)
		{
			Play(bLooping);
		}
	}
}

void UAnimSingleNodeInstance::SetAnimation(UAnimSequenceBase* InAnimation)
{
	if (CurrentAnimation == InAnimation && !NeedsBoneMappingRebuild())
	{
		CurrentTime = 0.0f;
		PreviousTime = 0.0f;
		bPoseDirty = true;
		return;
	}

	CurrentAnimation = InAnimation;
	SyncAnimationAssetPathFromAnimation(InAnimation);
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
	bPoseDirty = true;
	BuildBoneMapping();
}

void UAnimSingleNodeInstance::SetAnimationAssetPath(const FString& InAnimationAssetPath)
{
	AnimationAssetPath.SetPath(FPaths::Normalize(InAnimationAssetPath));
	ApplyAnimationFromAssetPath();
}

void UAnimSingleNodeInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
	UAnimInstance::Initialize(InOwnerComponent);
	BuildBoneMapping();
}

bool UAnimSingleNodeInstance::NeedsBoneMappingRebuild() const
{
	USkeletalMesh* CurrentMesh = OwnerComponent ? OwnerComponent->GetSkeletalMesh() : nullptr;
	return CachedMappingMesh != CurrentMesh || CachedMappingAnimation != CurrentAnimation;
}

//2. Bone Mapping Phase(UAnimSingleNodeInstance::NativeUpdateAnimation으로 이어짐)
//AnimSequence 의 Track 이름과 실제 렌더링될 skeletal mesh 의 Bone 이름을 비교해 mapping table을 생성합니다
void UAnimSingleNodeInstance::BuildBoneMapping()
{
	TrackToBoneMap.clear();

	USkeletalMesh* Mesh = OwnerComponent ? OwnerComponent->GetSkeletalMesh() : nullptr;
	CachedMappingMesh = Mesh;
	CachedMappingAnimation = CurrentAnimation;

	if (!Mesh || !CurrentAnimation)
	{
		return;
	}

	const TArray<FBoneAnimationTrack>& Tracks = CurrentAnimation->GetBoneAnimationTracks();
	TrackToBoneMap.resize(Tracks.size(), -1);

	const TArray<FBoneInfo>& Bones = Mesh->GetBones();
	TMap<FName, int32, FName::Hash> BoneNameToIndex;
	BoneNameToIndex.reserve(Bones.size());

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
	{
		BoneNameToIndex[FName(Bones[BoneIndex].Name)] = BoneIndex;
	}

	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
	{
		auto It = BoneNameToIndex.find(Tracks[TrackIndex].Name);
		if (It != BoneNameToIndex.end())
		{
			TrackToBoneMap[TrackIndex] = It->second;
		}
	}
}

void UAnimSingleNodeInstance::Play(bool bInLooping)
{
	bLooping = bInLooping;
	bPlaying = true;
	bPoseDirty = true;
}

void UAnimSingleNodeInstance::Stop()
{
	bPlaying = false;
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
	bPoseDirty = true;
}

void UAnimSingleNodeInstance::Pause()
{
	bPlaying = false;
}

void UAnimSingleNodeInstance::SetPosition(float InPosition)
{
	CurrentTime = InPosition;
	PreviousTime = InPosition;
	bPoseDirty = true;
}

void UAnimSingleNodeInstance::CopyPlaybackSettingsFrom(const UAnimSingleNodeInstance* SourceInstance)
{
	if (!SourceInstance)
	{
		return;
	}

	PlayRate = SourceInstance->PlayRate;
	bLooping = SourceInstance->bLooping;
	bAutoPlay = SourceInstance->bAutoPlay;

	if (bAutoPlay && CurrentAnimation)
	{
		Play(bLooping);
	}
	else
	{
		bPlaying = false;
	}
}

float UAnimSingleNodeInstance::GetLength() const
{
	return CurrentAnimation ? CurrentAnimation->GetPlayLength() : 0.0f;
}

void UAnimSingleNodeInstance::ApplyAnimationFromAssetPath()
{
	const FString RequestedPath = AnimationAssetPath.GetPath();
	if (RequestedPath.empty())
	{
		SetAnimation(nullptr);
		return;
	}

	UAnimSequenceBase* LoadedAnimation = Cast<UAnimSequenceBase>(
		FResourceManager::Get().LoadAnimSequence(RequestedPath));
	if (!LoadedAnimation)
	{
		CurrentAnimation = nullptr;
		AnimationAssetPath.SetPath(RequestedPath);
		CurrentTime = 0.0f;
		PreviousTime = 0.0f;
		bPlaying = false;
		bPoseDirty = true;
		BuildBoneMapping();
		if (OwnerComponent)
		{
			OwnerComponent->ResetToBindPose();
		}
		UE_LOG_WARNING("[AnimSingleNodeInstance] Failed to load animation asset: %s", RequestedPath.c_str());
		return;
	}

	SetAnimation(LoadedAnimation);
	if (bAutoPlay)
	{
		Play(bLooping);
	}
}

void UAnimSingleNodeInstance::SyncAnimationAssetPathFromAnimation(UAnimSequenceBase* Animation)
{
	if (!Animation)
	{
		AnimationAssetPath.SetPath("");
		return;
	}

	const FString PersistentPath = GetPersistentAnimationAssetPath(Animation);
	if (!PersistentPath.empty())
	{
		AnimationAssetPath.SetPath(PersistentPath);
	}
}

//3-1. Update Phase(UAnimSequence::GetAnimationPose로 이어짐)
//DeltaTime을 누적, CurrentTime 전진!
void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaTime)
{
	if (!bPlaying || !CurrentAnimation) return;

	const float OriginalTime = CurrentTime;
	PreviousTime = CurrentTime;
	CurrentTime += DeltaTime * PlayRate;

	const float Length = CurrentAnimation->GetPlayLength();

	bool bLooped = false;
	const bool bReverse = PlayRate < 0.0f;

	if (Length <= 0.0f)
	{
		CurrentTime = 0.0f;
		bPlaying = false;
		bPoseDirty = bPoseDirty || CurrentTime != OriginalTime;
		TriggerAnimNotifies(CurrentAnimation, PreviousTime, CurrentTime, bLooped, bReverse, DeltaTime);
		return;
	}

	if (!bReverse)  // 정방향 재생
	{
		if (CurrentTime > Length)
		{
			if (bLooping)
			{
				CurrentTime = std::fmod(CurrentTime, Length);
				bLooped = true;
			}
			else
			{
				CurrentTime = Length;
				bPlaying = false;
			}
		}
	}
	else    // 역방향 재생
	{
		if (CurrentTime < 0.0f)
		{
			if (bLooping)
			{
				CurrentTime = std::fmod(CurrentTime, Length);
				if (CurrentTime < 0.0f)
				{
					CurrentTime += Length;
				}
				bLooped = true;
			}
			else
			{
				CurrentTime = 0.0f;
				bPlaying = false;
			}
		}
	}

	bPoseDirty = bPoseDirty || CurrentTime != OriginalTime;
	TriggerAnimNotifies(CurrentAnimation, PreviousTime, CurrentTime, bLooped, bReverse, DeltaTime);
}

bool UAnimSingleNodeInstance::EvaluatePose(FPoseContext& OutPoseContext)
{
	if (!CurrentAnimation) return false;

	const bool bNeedsMappingRebuild = NeedsBoneMappingRebuild();
	if (!bPoseDirty && !bNeedsMappingRebuild)
	{
		return false;
	}

	if (bNeedsMappingRebuild)
	{
		BuildBoneMapping();
	}

	OutPoseContext.TrackToBoneMap = TrackToBoneMap;
	const bool bEvaluated = CurrentAnimation->GetAnimationPose(CurrentTime, OutPoseContext);
	if (bEvaluated)
	{
		bPoseDirty = false;
	}
	return bEvaluated;
}
