#include "AnimSequenceBase.h"
#include "Object/GarbageCollection.h"
void UAnimSequenceBase::AddReferencedObjects(FReferenceCollector& Collector)
{
    UObject::AddReferencedObjects(Collector);

    for (const FAnimNotifyEvent& Notify : Notifies)
    {
        Notify.AddReferencedObjects(Collector);
    }
}

void UAnimSequenceBase::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << PlayLength;
    Ar << FrameRate;
    Ar << Notifies;
}
