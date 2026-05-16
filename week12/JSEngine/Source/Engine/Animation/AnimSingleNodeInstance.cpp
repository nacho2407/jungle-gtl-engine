#include "AnimSingleNodeInstance.h"

#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"

DEFINE_CLASS(UAnimSingleNodeInstance, UAnimInstance)

void UAnimSingleNodeInstance::Serialize(FArchive& Ar)
{
    UAnimInstance::Serialize(Ar);
}

void UAnimSingleNodeInstance::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UAnimInstance::GetEditableProperties(OutProps);
}

void UAnimSingleNodeInstance::PostEditProperty(const char* PropertyName)
{
    UAnimInstance::PostEditProperty(PropertyName);
}

void UAnimSingleNodeInstance::SetAnimation(UAnimSequenceBase* InAnimation)
{
    if (CurrentAnimation == InAnimation && !NeedsBoneMappingRebuild())
    {
        CurrentTime = 0.0f;
        PreviousTime = 0.0f;
        return;
    }

    CurrentAnimation = InAnimation;
    CurrentTime = 0.0f;
    PreviousTime = 0.0f;
    BuildBoneMapping();
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
}

void UAnimSingleNodeInstance::Stop()
{
    bPlaying = false;
    CurrentTime = 0.0f;
    PreviousTime = 0.0f;
}

void UAnimSingleNodeInstance::Pause()
{
	bPlaying = false;
}

void UAnimSingleNodeInstance::SetPosition(float InPosition)
{
    CurrentTime = InPosition;
	PreviousTime = InPosition;
}

float UAnimSingleNodeInstance::GetLength() const
{
	return CurrentAnimation ? CurrentAnimation->GetPlayLength() : 0.0f;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaTime)
{
    if (!bPlaying || !CurrentAnimation) return;

    PreviousTime = CurrentTime;
    CurrentTime += DeltaTime * PlayRate;

    float Length = CurrentAnimation->GetPlayLength();

    bool bLooped = false;
    bool bReverse = PlayRate < 0.0f;

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
                CurrentTime = Length + std::fmod(CurrentTime, Length);
                bLooped = true;
            }
            else
            {
                CurrentTime = 0.0f; 
                bPlaying = false;
            }
        }
    }

    TriggerAnimNotifies(CurrentAnimation, PreviousTime, CurrentTime, bLooped, bReverse);
}

bool UAnimSingleNodeInstance::EvaluatePose(FPoseContext& OutPoseContext)
{
    if (!CurrentAnimation) return false;

    if (NeedsBoneMappingRebuild())
    {
        BuildBoneMapping();
    }

    OutPoseContext.TrackToBoneMap = TrackToBoneMap;
    return CurrentAnimation->GetAnimationPose(CurrentTime, OutPoseContext);
}