#include "Component/StaticMeshComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UStaticMeshComp, UMeshComponent)


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
	/*if (bIsLoading)
	{
		FString AssetName; 
		
	}*/
}
