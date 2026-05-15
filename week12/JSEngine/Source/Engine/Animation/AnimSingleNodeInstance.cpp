#include "AnimSingleNodeInstance.h"

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
    CurrentAnimation = InAnimation;
    CurrentTime = 0.0f;
    PreviousTime = 0.0f;
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
    return CurrentAnimation->GetAnimationPose(CurrentTime, OutPoseContext);
}