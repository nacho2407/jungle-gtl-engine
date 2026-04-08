#include "TextActor.h"
#include "Asset/ObjManager.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Component/TextComponent.h"

IMPLEMENT_RTTI(ATextActor, AActor)

void ATextActor::PostSpawnInitialize()
{
	TextComponent = FObjectFactory::ConstructObject<UTextRenderComponent>(this, "TextComponent");
	AddOwnedComponent(TextComponent);

	AActor::PostSpawnInitialize();
}