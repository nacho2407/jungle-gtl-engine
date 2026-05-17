#pragma once

#include "Core/CoreMinimal.h"

class UAnimSequence;
class USkeletalMesh;

class FFbxAnimSequenceImporter
{
public:
    UAnimSequence* LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, USkeletalMesh* TargetSkeletalMesh);

    UAnimSequence* LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, const FString& AnimStackName, USkeletalMesh* TargetSkeletalMesh);


    TArray<FString> ListAnimStacks(const FString& Path);
};
