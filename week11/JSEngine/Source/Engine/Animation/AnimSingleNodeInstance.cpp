#include "Animation/AnimSingleNodeInstance.h"

#include "Animation/AnimSequence.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
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

    if (!CurrentAsset)
    {
        bPlaying = false;
        bPaused = false;
    }
}

UAnimationAsset* UAnimSingleNodeInstance::GetAnimationAsset() const
{
    return CurrentAsset;
}

void UAnimSingleNodeInstance::Play()
{
    if (!CurrentAsset)
    {
        UE_LOG_WARNING("[AnimSingleNodeInstance] Play called without animation asset.");
        bPlaying = false;
        bPaused = false;
        return;
    }

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

    if (!CurrentAsset)
    {
        return false;
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
        return false;
    }

    UE_LOG_WARNING("[AnimSingleNodeInstance] CurrentAsset is not a supported sequence asset.");

    return false;
}
