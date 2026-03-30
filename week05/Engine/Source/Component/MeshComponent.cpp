#include "Object/Class.h"
#include "Serializer/Archive.h"
#include "Renderer/Material.h"
#include "MeshComponent.h"

#include "Debug/EngineLog.h"

IMPLEMENT_RTTI(UMeshComponent, UPrimitiveComponent)

void UMeshComponent::SetMaterial(int32 Index, const std::shared_ptr<FMaterial>& InMaterial)
{
	if (Index >= 0)
	{
		if (Index >= Materials.size())
		{
			Materials.resize(Index + 1, nullptr);
		}
		Materials[Index] = InMaterial;
	}
}

std::shared_ptr<FMaterial> UMeshComponent::GetMaterial(int32 Index) const
{
	if (Index >= 0 && Index < Materials.size()) return Materials[Index];
	return nullptr;
}

/*
void UMeshComponent::Serialize(FArchive& Ar)
{
	UUPrimitiveComponent::Serialize(Ar);

	uint32 MatCount = static_cast<uint32>(Materials.size());
	Ar.Serialize("MaterialCount", MatCount);

	if (!Ar.IsSaving())
	{
		Materials.resize(MatCount, nullptr);
	}

	for (uint32 i = 0; i < MatCount; ++i)
	{
		FString MatName;
		MatName = Materials[0]->GetOriginName();
		FString KeyName = FString("Material_") + std::to_string(i).c_str();
		Ar.Serialize(KeyName, MatName);

		if (!Ar.IsSaving() && !MatName.empty())
		{
			// TODO: 나중에 머티리얼 매니저가 생기면 주석 해제
			// Materials[i] = FMaterialManager::LoadMaterial(MatName);
		}
	}
}
*/