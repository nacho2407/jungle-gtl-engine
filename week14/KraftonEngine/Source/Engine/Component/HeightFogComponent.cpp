#include "HeightFogComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Render/Proxy/FScene.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UHeightFogComponent, USceneComponent)

UHeightFogComponent::UHeightFogComponent()
{
	SetComponentTickEnabled(false);
}

void UHeightFogComponent::CreateRenderState()
{
	PushToScene();
}

void UHeightFogComponent::DestroyRenderState()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().GetEnvironment().RemoveFog(this);
}

void UHeightFogComponent::OnTransformDirty()
{
	USceneComponent::OnTransformDirty();
	PushToScene();
}

void UHeightFogComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	FFogParams Params;
	Params.Density = FogDensity;
	Params.HeightFalloff = FogHeightFalloff;
	Params.StartDistance = StartDistance;
	Params.CutoffDistance = FogCutoffDistance;
	Params.MaxOpacity = FogMaxOpacity;
	Params.FogBaseHeight = GetWorldLocation().Z;
	Params.InscatteringColor = FogInscatteringColor;

	World->GetScene().GetEnvironment().AddFog(this, Params);
}

void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);

	//                                                                     Min      Max        Speed
	OutProps.push_back({ "Fog Density",       EPropertyType::Float,  &FogDensity,        0.0f, 0.05f,     0.001f });
	OutProps.push_back({ "Height Falloff",    EPropertyType::Float,  &FogHeightFalloff,  0.001f, 5.0f,    0.01f });
	OutProps.push_back({ "Start Distance",    EPropertyType::Float,  &StartDistance,     0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Cutoff Distance",   EPropertyType::Float,  &FogCutoffDistance, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Max Opacity",       EPropertyType::Float,  &FogMaxOpacity,     0.0f, 1.0f,      0.01f });
	OutProps.push_back({ "Inscattering Color", EPropertyType::Color4, &FogInscatteringColor });
}

void UHeightFogComponent::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);
	PushToScene();
}

void UHeightFogComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);

	Ar << FogDensity;
	Ar << FogHeightFalloff;
	Ar << StartDistance;
	Ar << FogCutoffDistance;
	Ar << FogMaxOpacity;
	Ar << FogInscatteringColor;
}

UBillboardComponent* UHeightFogComponent::EnsureEditorBillboard()
{
	if (!Owner)
	{
		return nullptr;
	}

	for (USceneComponent* Child : GetChildren())
	{
		UBillboardComponent* Billboard = Cast<UBillboardComponent>(Child);
		if (Billboard && Billboard->IsEditorOnlyComponent())
		{
			// 에디터 아이콘 빌보드는 부모 스케일과 컴포넌트 트리 기본 표시에서 분리한다.
			Billboard->SetAbsoluteScale(true);
			Billboard->SetHiddenInComponentTree(true);
			return Billboard;
		}
	}

	UBillboardComponent* Billboard = Owner->AddComponent<UBillboardComponent>();
	if (Billboard)
	{
		Billboard->AttachToComponent(this);
		// 에디터 아이콘 빌보드는 부모 스케일과 컴포넌트 트리 기본 표시에서 분리한다.
		Billboard->SetAbsoluteScale(true);
		Billboard->SetEditorOnlyComponent(true);
		Billboard->SetHiddenInComponentTree(true);
		auto Material = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/Editor/HeightFog.mat");
		Billboard->SetMaterial(Material);
	}

	return Billboard;
}
