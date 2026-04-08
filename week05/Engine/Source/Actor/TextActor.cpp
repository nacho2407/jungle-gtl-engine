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

void ATextActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor::FixupDuplicatedReferences(DuplicatedObject, Context);
	static_cast<ATextActor*>(DuplicatedObject)->TextComponent = Context.FindDuplicate(TextComponent);
}
