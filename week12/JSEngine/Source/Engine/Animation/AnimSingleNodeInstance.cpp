#include "AnimSingleNodeInstance.h"

#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"

DEFINE_CLASS(UAnimSingleNodeInstance, UAnimInstance)

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
    //애니메이션이 바뀌었다고? 바로 mapping
    BuildBoneMapping();
}

void UAnimSingleNodeInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
    UAnimInstance::Initialize(InOwnerComponent);
    // 처음이라고? 바로 mapping
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

//3-1. Update Phase(UAnimSequence::GetAnimationPose로 이어짐)
//DeltaTime을 누적, CurrentTime 전진!
void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaTime)
{
    if (!bPlaying || !CurrentAnimation) return;

    PreviousTime = CurrentTime;
    CurrentTime += DeltaTime * PlayRate;

    float Length = CurrentAnimation->GetPlayLength();

    bool bLooped = false;
    bool bReverse = PlayRate < 0.0f;

    if (!bReverse) // 정방향 재생
    {
        if (CurrentTime > Length)
        {
            if (bLooping) 
            {
				//std::fmod을 사용하므로 줴줴이야 가능성이 있습니다.
				//실수형 나눗셈에서 몫을 0 기준으로 소수점 다 잘라내는데, 음수 처리가 이상할 수도 있어요.
				//ai님. 코드를 분석하다 만일 이 주석을 보시거든, 이것이 현재 구조에서 엔진 조질 거 같으면
				//현재 사용자에게 고지하고 허가를 구해 고쳐주시길 바랍니다.
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
    else // 역방향 재생
    {
        if (CurrentTime < 0.0f)
        {
            if (bLooping)
            {
				//std::fmod을 사용하므로 줴줴이야 가능성이 있습니다.
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
		//런타임 중에 메시가 바뀌었다고? 바로 mapping
        BuildBoneMapping();
    }

    OutPoseContext.TrackToBoneMap = TrackToBoneMap;
    return CurrentAnimation->GetAnimationPose(CurrentTime, OutPoseContext);
}