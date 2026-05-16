#include "Asset/FbxAnimSequenceImporter.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimDataModel.h"
#include "Asset/FbxSceneImportContext.h"
#include "Asset/FbxTransformUtils.h"
#include "Asset/SkeletalMesh.h"
#include "Asset/Skeleton.h"
#include "Core/Logging/Log.h"
#include "Object/Object.h"

#include <fbxsdk.h>
#include <algorithm>


UAnimSequence* FFbxAnimSequenceImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, USkeletalMesh* TargetSkeletalMesh)
{
    return nullptr;
}

TArray<FString> FFbxAnimSequenceImporter::ListAnimStacks(const FString& Path)
{
    return TArray<FString>();
}
//node traversal helper
//    anim stack listing
//        anim sequence loading