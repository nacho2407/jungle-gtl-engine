#include "BillboardActor.h"
#include "Asset/ObjManager.h"
#include "Object/ObjectFactory.h"
#include "Core/Paths.h"
#include "Object/Class.h"
#include "Component/BillboardComponent.h"

IMPLEMENT_RTTI(ABillboardActor, AActor)

void ABillboardActor::PostSpawnInitialize()
{
	BillboardComponent = FObjectFactory::ConstructObject<UBillboardComponent>(this, "BillboardComponent");
	AddOwnedComponent(BillboardComponent);

	BillboardComponent->SetTexturePath((FPaths::IconDir() / L"Pawn_64x.png").wstring());

	AActor::PostSpawnInitialize();
}
