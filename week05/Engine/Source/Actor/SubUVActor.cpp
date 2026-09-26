#include "SubUVActor.h"
#include "Asset/ObjManager.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Component/SubUVComponent.h"

IMPLEMENT_RTTI(ASubUVActor, AActor)

void ASubUVActor::PostSpawnInitialize()
{
	SubUVComponent = FObjectFactory::ConstructObject<USubUVComponent>(this, "SubUVComponent");
	AddOwnedComponent(SubUVComponent);

	AActor::PostSpawnInitialize();
}

void ASubUVActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor::FixupDuplicatedReferences(DuplicatedObject, Context);
	static_cast<ASubUVActor*>(DuplicatedObject)->SubUVComponent = Context.FindDuplicate(SubUVComponent);
}
