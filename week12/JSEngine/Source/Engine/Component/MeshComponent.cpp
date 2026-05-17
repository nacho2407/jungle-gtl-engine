#include "MeshComponent.h"
#include "Render/Resource/Material.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <filesystem>


void UMeshComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	SerializeMaterialOverrides(Ar);
}

void UMeshComponent::SerializeMaterialOverrides(FArchive& Ar)
{
	TArray<FString> MaterialPaths;

	if (Ar.IsLoading())
	{
		Ar << "Materials" << MaterialPaths;

		Materials.resize(MaterialPaths.size());
		for (int32 i = 0; i < static_cast<int32>(MaterialPaths.size()); ++i)
		{
			if (!MaterialPaths[i].empty())
			{
				SetMaterial(i, FResourceManager::Get().GetMaterialInterface(MaterialPaths[i]));
			}
			else
			{
				Materials[i] = nullptr;
			}
		}
	}
	else if (Ar.IsSaving())
	{
		for (auto& Mat : Materials)
		{
			if (UMaterialInstance* MatInst = Cast<UMaterialInstance>(Mat))
			{
				MaterialPaths.push_back(FPaths::Normalize(MatInst->GetFilePath()));
			}
			else if (UMaterial* BaseMat = Cast<UMaterial>(Mat))
			{
				const std::filesystem::path FilePath(FPaths::ToWide(BaseMat->GetFilePath()));
				const bool bFileBackedMaterial = FilePath.extension() == L".mat";
				MaterialPaths.push_back(bFileBackedMaterial ? FPaths::Normalize(BaseMat->GetFilePath()) : BaseMat->GetName());
			}
			else
			{
				MaterialPaths.push_back(Mat ? Mat->GetName() : "");
			}
		}
		Ar << "Materials" << MaterialPaths;
	}
}

void UMeshComponent::SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial)
{
	if (SlotIndex < 0)
	{
		return;
	}
	
	if (SlotIndex >= static_cast<int32>(Materials.size()))
	{
		Materials.resize(SlotIndex + 1, nullptr);
	}

	Materials[SlotIndex] = InMaterial;
}

UMaterialInterface* UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(Materials.size()))
	{
		return nullptr;
	}
	
	return Materials[SlotIndex];
}

const TArray<UMaterialInterface*>& UMeshComponent::GetOverrideMaterial() const
{
	return Materials;
}

int32 UMeshComponent::GetNumMaterials() const
{
	return static_cast<int32>(Materials.size());
}

void UMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UMeshComponent::TickComponent(float DeltaTime)
{
	//ScrollV += DeltaTime;

	//if (ScrollU >= 1.f) ScrollU = 0.f;
}

