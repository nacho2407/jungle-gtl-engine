#include "LightComponentBase.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Intensity",EPropertyType::Float,&Intensity,0.0f,50.f,0.05f });
	OutProps.push_back({ "Color",EPropertyType::Color4,&LightColor });
	OutProps.push_back({ "Visible",EPropertyType::Bool,&bVisible });
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << Intensity;
	Ar << LightColor;
	Ar << bVisible;
}

UBillboardComponent* ULightComponentBase::EnsureEditorBillboard()
{
	if (!Owner)
	{
		return nullptr;
	}

	const char* IconMaterialPath = nullptr;
	switch (GetLightType())
	{
	case ELightComponentType::Ambient:
		IconMaterialPath = "Asset/Materials/Editor/AmbientLight.mat";
		break;
	case ELightComponentType::Directional:
		IconMaterialPath = "Asset/Materials/Editor/DirectionalLight.mat";
		break;
	case ELightComponentType::Point:
		IconMaterialPath = "Asset/Materials/Editor/PointLight.mat";
		break;
	case ELightComponentType::Spot:
		IconMaterialPath = "Asset/Materials/Editor/SpotLight.mat";
		break;
	}

	if (!IconMaterialPath)
	{
		return nullptr;
	}

	UBillboardComponent* Billboard = Owner->AddComponent<UBillboardComponent>();
	if (Billboard)
	{
		Billboard->SetEditorOnly(true);
		auto Material = FMaterialManager::Get().GetOrCreateMaterial(IconMaterialPath);
		Billboard->SetMaterial(Material);
		Billboard->AttachToComponent(this);
	}

	return Billboard;
}
