#include "Component/StaticMeshComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"

IMPLEMENT_CLASS(UStaticMeshComp, UMeshComponent)


void UStaticMeshComp::SetStaticMesh(UStaticMesh* InMesh)
{
	StaticMesh = InMesh;
	if (InMesh)
		StaticMeshPath = InMesh->GetAssetPathFileName();
	else
		StaticMeshPath = "None";
}

UStaticMesh* UStaticMeshComp::GetStaticMesh() const
{
	return StaticMesh;
}

void UStaticMeshComp::Serialize(bool bIsLoading, json::JSON& Handle)
{
}

void UStaticMeshComp::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Static Mesh", EPropertyType::StaticMeshRef, &StaticMeshPath });
}

void UStaticMeshComp::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "Static Mesh") == 0)
	{
		if (StaticMeshPath.empty() || StaticMeshPath == "None")
		{
			StaticMesh = nullptr;
		}
		else
		{
			UStaticMesh* Loaded = FObjManager::LoadObjStaticMesh(StaticMeshPath);
			StaticMesh = Loaded;
		}
	}
}
