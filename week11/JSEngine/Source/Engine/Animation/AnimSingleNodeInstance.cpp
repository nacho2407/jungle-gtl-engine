#include "Animation/AnimSingleNodeInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationRuntime.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Stats.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
REGISTER_FACTORY(UAnimSingleNodeInstance)

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimationAsset* NewAsset)
{
    if (CurrentAsset == NewAsset)
    {
        return;
    }

    CurrentAsset = NewAsset;
    CurrentTime = 0.0f;
}

UAnimationAsset* UAnimSingleNodeInstance::GetAnimationAsset() const
{
    return CurrentAsset;
}

void UAnimSingleNodeInstance::Play()
{
    bPlaying = true;
    bPaused = false;
}

void UAnimSingleNodeInstance::Pause()
{
    if (bPlaying)
    {
        bPaused = true;
    }
}

void UAnimSingleNodeInstance::Stop()
{
    bPlaying = false;
    bPaused = false;
    CurrentTime = 0.0f;
}

void UAnimSingleNodeInstance::SetPosition(float InTimeSeconds, bool bFireNotifies)
{
    (void)bFireNotifies;
    CurrentTime = InTimeSeconds >= 0.0f ? InTimeSeconds : 0.0f;
}

float UAnimSingleNodeInstance::GetPosition() const
{
    return CurrentTime;
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
    PlayRate = InPlayRate;
}

float UAnimSingleNodeInstance::GetPlayRate() const
{
    return PlayRate;
}

void UAnimSingleNodeInstance::SetLooping(bool bInLooping)
{
    bLooping = bInLooping;
}

bool UAnimSingleNodeInstance::IsLooping() const
{
    return bLooping;
}

bool UAnimSingleNodeInstance::IsPlaying() const
{
    return bPlaying;
}

bool UAnimSingleNodeInstance::IsPaused() const
{
    return bPaused;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    if (!bPlaying || bPaused)
    {
        return;
    }

    CurrentTime += DeltaSeconds * PlayRate;
    if (CurrentTime < 0.0f)
    {
        CurrentTime = 0.0f;
        bPlaying = false;
        bPaused = false;
    }
}

bool UAnimSingleNodeInstance::EvaluateAnimation(TArray<FTransform>& OutLocalPose)
{
    SCOPE_STAT("Anim.EvaluatePose");

    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;

    if (!Mesh)
    {
        return false;
    }

    // animation asset이 아예 없는 경우에만 milestone 1 mock pose를 허용
    if (!CurrentAsset)
    {
        if (FAnimationRuntime::BuildDebugOscillatingLocalPose(Mesh, CurrentTime, OutLocalPose))
        {
            return true;
        }

        return FAnimationRuntime::BuildBindLocalPoseFromMesh(Mesh, OutLocalPose);
    }

    // asset이 있으면 실제 sequence 평가를 시도
    if (UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentAsset))
    {
        const FAnimExtractContext ExtractContext(CurrentTime, bLooping);
        if (Sequence->GetAnimationPose(OutLocalPose, Mesh, ExtractContext))
        {
            return true;
        }

        UE_LOG_WARNING("[AnimSingleNodeInstance] Failed to evaluate animation sequence.");
        return FAnimationRuntime::BuildBindLocalPoseFromMesh(Mesh, OutLocalPose);
    }

    // asset은 있는데 single node가 지원하지 않는 타입이면 bind pose fallback
    UE_LOG_WARNING("[AnimSingleNodeInstance] CurrentAsset is not a supported sequence asset.");

    return FAnimationRuntime::BuildBindLocalPoseFromMesh(Mesh, OutLocalPose);
}
