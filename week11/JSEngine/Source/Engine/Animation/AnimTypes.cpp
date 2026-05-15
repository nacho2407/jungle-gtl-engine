#include "AnimTypes.h"
#include "Asset/SkeletalMesh.h"
#include "Asset/SkeletalMeshTypes.h"


bool UAnimSequence::GetBonePose(float Time, const USkeletalMesh* Mesh, TArray<FMatrix>& OutLocalPose) const
{
    if (!Mesh || !DataModel)
        return false;

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    OutLocalPose.resize(BoneCount);
    return true;
}
