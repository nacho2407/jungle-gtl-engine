#include "AnimDataModel.h"

DEFINE_CLASS(UAnimDataModel, UObject)

void UAnimDataModel::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << PlayLength;
    Ar << FrameRate;
    Ar << NumFrames;
    Ar << BoneAnimationTracks;
    Ar << Notifies;
}
