#include "Object/Class.h"
#include "Serializer/Archive.h"
#include "Renderer/Material.h"
#include "MeshComponent.h"

#include "Debug/EngineLog.h"
#include "Renderer/MaterialManager.h"

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
		MarkRenderStateDirty();
	}
}

std::shared_ptr<FMaterial> UMeshComponent::GetMaterial(int32 Index) const
{
	if (Index >= 0 && Index < Materials.size()) return Materials[Index];
	return nullptr;
}

std::shared_ptr<FDynamicMaterial> UMeshComponent::GetOrCreateDynamicMaterial(int32 Index)
{
	if (Index < 0 || Index >= static_cast<int32>(Materials.size()))
	{
		return nullptr;
	}

	std::shared_ptr<FMaterial>& Material = Materials[Index];
	if (!Material)
	{
		return nullptr;
	}

	if (std::shared_ptr<FDynamicMaterial> DynamicMaterial = std::dynamic_pointer_cast<FDynamicMaterial>(Material))
	{
		return DynamicMaterial;
	}

	std::unique_ptr<FDynamicMaterial> DynamicClone = Material->CreateDynamicMaterial();
	if (!DynamicClone)
	{
		return nullptr;
	}

	std::shared_ptr<FDynamicMaterial> SharedDynamicMaterial(std::move(DynamicClone));
	Material = SharedDynamicMaterial;
	MarkRenderStateDirty();
	return SharedDynamicMaterial;
}

void UMeshComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		TArray<FString> MaterialNames;
		for (const std::shared_ptr<FMaterial>& Material : Materials)
		{
			if (Material) MaterialNames.push_back(Material->GetOriginName());
			else MaterialNames.push_back("");
		}
		Ar.SerializeStringArray("Materials", MaterialNames);
	}
	else
	{
		if (Ar.Contains("Materials"))
		{
			TArray<FString> MaterialNames;
			Ar.SerializeStringArray("Materials", MaterialNames);

			Materials.clear();
			for (const FString& MaterialName : MaterialNames)
			{
				if (!MaterialName.empty())
				{
					std::shared_ptr<FMaterial> LoadedMaterial = FMaterialManager::Get().FindByName(MaterialName);
					Materials.push_back(LoadedMaterial);
				}
				else Materials.push_back(nullptr);
			}
			MarkRenderStateDirty();
		}
	}
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
