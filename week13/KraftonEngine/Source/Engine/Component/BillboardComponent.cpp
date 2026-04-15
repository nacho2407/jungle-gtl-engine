#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Proxy/BillboardSceneProxy.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <cstring>

IMPLEMENT_CLASS(UBillboardComponent, UPrimitiveComponent)

FPrimitiveSceneProxy* UBillboardComponent::CreateSceneProxy()
{
	return new FBillboardSceneProxy(this);
}

void UBillboardComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << bIsBillboard;
	Ar << MaterialSlot.Path;
	Ar << Width;
	Ar << Height;
}

void UBillboardComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	if (!MaterialSlot.Path.empty() && MaterialSlot.Path != "None")
	{
		UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot.Path);
		if (LoadedMat)
		{
			SetMaterial(LoadedMat);
		}
	}
}

void UBillboardComponent::SetMaterial(UMaterial* InMaterial)
{
	Material = InMaterial;
	if (Material)
	{
		MaterialSlot.Path = Material->GetAssetPathFileName();
	}
	else
	{
		MaterialSlot.Path = "None";
	}
	// 머티리얼 변경 시 렌더 스테이트와 프록시 갱신
	MarkProxyDirty(EDirtyFlag::Material);
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UBillboardComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Material", EPropertyType::MaterialSlot, &MaterialSlot });
	OutProps.push_back({ "Width",  EPropertyType::Float, &Width,  0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
}

void UBillboardComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Material") == 0)
	{
		if (MaterialSlot.Path == "None" || MaterialSlot.Path.empty())
		{
			SetMaterial(nullptr);
		}
		else
		{
			UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot.Path);
			if (LoadedMat)
			{
				SetMaterial(LoadedMat);
			}
		}
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Width") == 0 || strcmp(PropertyName, "Height") == 0)
	{
		// Width/Height는 빌보드 쿼드 크기를 결정하므로 트랜스폼/월드 바운드 모두 갱신해야 한다.
		MarkProxyDirty(EDirtyFlag::Transform);
		MarkWorldBoundsDirty();
	}
}

void UBillboardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (!GetOwner() || !GetOwner()->GetWorld()) return;

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
	if (!ActiveCamera) return;

	FVector WorldLocation = GetWorldLocation();
	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}

FMatrix UBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
	// TickComponent와 동일한 로직
	FVector Forward = (CameraForward * -1.0f).Normalized();
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f);
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	return FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}
