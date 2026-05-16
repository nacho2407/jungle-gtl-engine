#include "AnimSequence.h"

#include "Asset/SkeletalMesh.h"
#include "Engine/Geometry/Transform.h"
#include <algorithm>
DEFINE_CLASS(UAnimSequence, UObject)

UAnimSequence::~UAnimSequence()
{
    if (DataModel)
    {
        delete DataModel;
        DataModel = nullptr;
    }
}


bool UAnimSequence::GetBonePose(float Time, const USkeletalMesh* Mesh, TArray<FMatrix>& OutLocalPose) const
{
    if (!Mesh || !DataModel)
        return false;

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0)
        return false;
    float EvalTime = Time;
    if (DataModel->SequenceLength > 0.f)
        EvalTime = std::clamp(EvalTime, 0.0f, DataModel->SequenceLength);
    OutLocalPose.resize(BoneCount);
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; BoneIndex++)
    {
        OutLocalPose[BoneIndex] = Bones[BoneIndex].LocalBindTransform;
    }
    for (const FAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        if (Track.BoneIndex < 0 || Track.BoneIndex >= BoneCount)
            continue;
        const FRawAnimSequenceTrack& Raw = Track.InternalTrackData;

        const FVector Pos = EvalVectorKeys(Raw.PosKeys, Raw.PosKeyTimes, EvalTime,FVector::ZeroVector);

        const FQuat Rot = EvalQuatKeys(Raw.RotKeys, Raw.RotKeyTimes,EvalTime,FQuat::Identity);

        const FVector Scale = EvalVectorKeys(Raw.ScaleKeys, Raw.ScaleKeyTimes, EvalTime, FVector::OneVector);

        OutLocalPose[Track.BoneIndex] = FTransform(Rot, Pos, Scale).ToMatrixWithScale();
    }
    return true;
}

FVector UAnimSequence::EvalVectorKeys(const TArray<FVector>& Keys, const TArray<float>& Times, float Time, const FVector& DefaultValue)
{
    if (Keys.empty())
    {
        return DefaultValue;
    }
    if (Keys.size() == 1 || Times.size() != Keys.size())
    {
        return Keys[0];
    }
    if (Time <= Times.front())
    {
        return Keys.front();
    }
    if (Time >= Times.back())
    {
        return Keys.back();
    }
    for (int32 i = 0 ; i+1 < static_cast<int32>(Times.size());i++)
    {
        if (Time >= Times[i] && Time <= Times[i + 1])
        {
            const float Range = Times[i + 1] - Times[i];
            const float Alpha = Range > 1.0e-6f ? (Time - Times[i]) / Range : 0.0f;
            return Keys[i] * (1.0f - Alpha) + Keys[i + 1] * Alpha;
        }
    }
    return Keys.back();
}

FQuat UAnimSequence::EvalQuatKeys(const TArray<FQuat>& Keys, const TArray<float>& Times, float Time, const FQuat& DefaultValue)
{
    if (Keys.empty())
    {
        return DefaultValue;
    }

    if (Keys.size() == 1 || Times.size() != Keys.size())
    {
        FQuat quat = Keys[0];
        quat.Normalize();
        return quat;
    }
    if (Time <= Times.front())
    {
        FQuat quat = Keys.front();
        quat.Normalize();
        return quat;
    }

    if (Time >= Times.back())
    {
        FQuat quat = Keys.back();
        quat.Normalize();
        return quat;
    }

	for (int32 i = 0; i + 1 < static_cast<int32>(Times.size()); i++)
    {
        if (Time >= Times[i] && Time <= Times[i + 1])
        {
            const float Range = Times[i + 1] - Times[i];
            const float Alpha = Range > 1.0e-6f ? (Time - Times[i]) / Range : 0.0f;
            FQuat x = Keys[i];
            FQuat y = Keys[i + 1];

            if (FQuat::DotProduct(x, y) < 0.0f)
            {
                y = y * -1.0f;
            }
            FQuat quat = FQuat::Slerp(x, y, Alpha);
            quat.Normalize();
            return quat;
        }

    }
    FQuat quat = Keys.back();
    quat.Normalize();
    return quat;
}
