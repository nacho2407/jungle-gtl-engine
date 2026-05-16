#pragma once

#include "Core/CoreMinimal.h"

class UAnimSequence;
class USkeletalMesh;

class FFbxAnimSequenceImporter
{
public:
    UAnimSequence* LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, USkeletalMesh* TargetSkeletalMesh);

    TArray<FString> ListAnimStacks(const FString& Path);
};
