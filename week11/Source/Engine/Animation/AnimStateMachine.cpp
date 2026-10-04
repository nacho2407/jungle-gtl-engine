#include "Animation/AnimStateMachine.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(UAnimStateMachine, UAnimationAsset)
REGISTER_FACTORY(UAnimStateMachine)

void UAnimStateMachine::SetDesc(const FAnimStateMachineDesc& InDesc)
{
    Desc = InDesc;
}

const FAnimStateMachineDesc& UAnimStateMachine::GetDesc() const
{
    return Desc;
}

FAnimStateMachineDesc& UAnimStateMachine::GetMutableDesc()
{
    return Desc;
}

void UAnimStateMachine::SetAssetPath(const FString& InPath)
{
    AssetPath = InPath;
}

const FString& UAnimStateMachine::GetAssetPath() const
{
    return AssetPath;
}
