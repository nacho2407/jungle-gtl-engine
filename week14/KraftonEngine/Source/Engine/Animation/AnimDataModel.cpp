#include "AnimDataModel.h"
void UAnimDataModel::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << PlayLength;
    Ar << FrameRate;
    Ar << NumFrames;
    Ar << BoneAnimationTracks;
    Ar << Notifies;
}
