#include "Component/StaticMeshComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UStaticMeshComp, UMeshComponent)
REGISTER_FACTORY(UStaticMeshComp)


void UStaticMeshComp::SetStaticMesh(UStaticMesh* InMesh)
{
	StaticMesh = InMesh;
}

UStaticMesh* UStaticMeshComp::GetStaticMesh() const
{
	return StaticMesh;
}

void UStaticMeshComp::Serialize(bool bIsLoading, json::JSON& Handle)
{
}
