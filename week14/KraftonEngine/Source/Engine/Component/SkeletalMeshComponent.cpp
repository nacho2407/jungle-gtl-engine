#include "SkeletalMeshComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/PoseContext.h"
#include "Mesh/SkeletalMesh.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include "Animation/AnimSingleNodeInstance.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    if (SingleNodeInstance)
    {
        UObjectManager::Get().DestroyObject(SingleNodeInstance);
        SingleNodeInstance = nullptr;
    }
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    UAnimSingleNodeInstance* Instance = GetOrCreateSingleNodeInstance();

    Instance->SetAnimationAsset(NewAnimToPlay);
    Instance->SetLooping(bLooping);
    Instance->SetPlaying(NewAnimToPlay != nullptr);
}

void USkeletalMeshComponent::StopAnimation()
{
    if (SingleNodeInstance)
    {
        SingleNodeInstance->SetAnimationAsset(nullptr);
        SingleNodeInstance->SetPlaying(false);
        SingleNodeInstance->SetCurrentTime(0.0f);
    }
}

UAnimSequenceBase* USkeletalMeshComponent::GetAnimation() const
{
    return SingleNodeInstance ? SingleNodeInstance->GetAnimationAsset() : nullptr;
}

UAnimInstance* USkeletalMeshComponent::GetAnimInstance() const
{
    return SingleNodeInstance;
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetAnimNodeInstance(FName NodeName) const
{
    return SingleNodeInstance;
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetOrCreateSingleNodeInstance()
{
    if (!SingleNodeInstance) SingleNodeInstance = UObjectManager::Get().CreateObject<UAnimSingleNodeInstance>(this);
    SingleNodeInstance->SetOwningComponent(this);
    SingleNodeInstance->NativeInitializeAnimation();

    return SingleNodeInstance;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    if (SingleNodeInstance && SingleNodeInstance->GetAnimationAsset())
    {
        if (SingleNodeInstance->IsPlaying())
        {
            SingleNodeInstance->UpdateAnimation(DeltaTime);
        }

        FPoseContext PoseContext;
        PoseContext.SkeletalMesh = GetSkeletalMesh();
        PoseContext.ResetToRefPose();

        SingleNodeInstance->EvaluatePose(PoseContext);

        if (!PoseContext.Pose.empty())
        {
            SetBoneLocalTransforms(PoseContext.Pose);
        }
    }
    
    USkinnedMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
