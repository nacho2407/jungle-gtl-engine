#include "AnimSequenceBase.h"

DEFINE_CLASS(UAnimSequenceBase, UObject)

void UAnimSequenceBase::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << PlayLength;
    Ar << FrameRate;
    Ar << Notifies;
}
