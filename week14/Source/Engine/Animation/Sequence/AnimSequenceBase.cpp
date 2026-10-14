#include "AnimSequenceBase.h"
void UAnimSequenceBase::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << PlayLength;
    Ar << FrameRate;
    Ar << Notifies;
}
