#include "AnimNotify_LogMessage.h"

#include "Core/Log.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UAnimNotify_LogMessage, UAnimNotify)

void UAnimNotify_LogMessage::Notify(USkeletalMeshComponent* /*MeshComp*/, UAnimSequenceBase* /*Anim*/)
{
	UE_LOG("[AnimNotify_LogMessage] %s", Message.c_str());
}
