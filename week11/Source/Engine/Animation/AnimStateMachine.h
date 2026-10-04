#pragma once

#include "Animation/AnimationAsset.h"
#include "Animation/AnimStateMachineTypes.h"

class UAnimStateMachine : public UAnimationAsset
{
public:
    DECLARE_CLASS(UAnimStateMachine, UAnimationAsset)

    UAnimStateMachine() = default;
    ~UAnimStateMachine() override = default;

    void SetDesc(const FAnimStateMachineDesc& InDesc);
    const FAnimStateMachineDesc& GetDesc() const;
    FAnimStateMachineDesc& GetMutableDesc();

    void SetAssetPath(const FString& InPath);
    const FString& GetAssetPath() const;

private:
    FAnimStateMachineDesc Desc;
    FString AssetPath;
};
