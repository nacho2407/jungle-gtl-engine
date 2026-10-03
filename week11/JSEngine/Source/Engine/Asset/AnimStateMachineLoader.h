#pragma once

#include "Asset/IAssetLoader.h"
#include "Animation/AnimStateMachineTypes.h"

class UAnimStateMachine;

class FAnimStateMachineLoader : public IAssetLoader
{
public:
    UAnimStateMachine* Load(const FString& Path) const;
    bool Save(const FString& Path, const UAnimStateMachine* StateMachine) const;
    bool LoadDescForEditor(const FString& Path, FAnimStateMachineDesc& OutDesc) const;
    bool SaveDescForEditor(const FString& Path, const FAnimStateMachineDesc& Desc) const;
    bool ValidateDescForRuntime(const FAnimStateMachineDesc& Desc, const FString& Path = "") const;
    FString BuildDescJsonForEditor(const FAnimStateMachineDesc& Desc) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;

private:
    bool ReadDesc(const FString& Path, FAnimStateMachineDesc& OutDesc) const;
    bool ReadDescInternal(const FString& Path, FAnimStateMachineDesc& OutDesc, bool bValidateRuntime, bool bAllowDraft) const;
    bool WriteDesc(const FString& Path, const FAnimStateMachineDesc& Desc) const;
    bool WriteDescInternal(const FString& Path, const FAnimStateMachineDesc& Desc, bool bValidateRuntime) const;
};
