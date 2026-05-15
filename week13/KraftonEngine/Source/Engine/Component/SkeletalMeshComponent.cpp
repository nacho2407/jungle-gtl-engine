#include "SkeletalMeshComponent.h"

#include "Animation/AnimExtractContext.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/PoseContext.h"
#include "Mesh/SkeletalMesh.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include <cmath>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    AnimationToPlay = NewAnimToPlay;
    CurrentAnimTime = 0.0f;
    bLoopAnimation = bLooping;
    bPlayingAnimation = AnimationToPlay != nullptr;
}

void USkeletalMeshComponent::StopAnimation()
{
    AnimationToPlay = nullptr;
    CurrentAnimTime = 0.0f;
    bPlayingAnimation = false;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    if (bPlayingAnimation && AnimationToPlay)
    {
        CurrentAnimTime += DeltaTime * PlayRate;

        const float Length = AnimationToPlay->GetPlayLength();
        if (Length > 0.0f)
        {
            if (bLoopAnimation)
            {
                CurrentAnimTime = std::fmod(CurrentAnimTime, Length);
                if (CurrentAnimTime < 0.0f)
                {
                    CurrentAnimTime += Length;
                }
            }
            else if (CurrentAnimTime > Length)
            {
                CurrentAnimTime = Length;
                bPlayingAnimation = false;
            }
        }

        FPoseContext PoseContext;
        PoseContext.SkeletalMesh = GetSkeletalMesh();
        PoseContext.ResetToRefPose();

        FAnimExtractContext ExtractContext;
        ExtractContext.CurrentTime = CurrentAnimTime;
        ExtractContext.bLooping = bLoopAnimation;
        ExtractContext.bExtractRootMotion = false;

        AnimationToPlay->GetBonePose(PoseContext, ExtractContext);

        if (!PoseContext.Pose.empty())
        {
            SetBoneLocalTransforms(PoseContext.Pose);
        }
    }

    USkinnedMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
