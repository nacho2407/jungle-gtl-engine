#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Proxy/BillboardSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"

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
	Ar << bIgnoreOwnerScale;
	Ar << TextureName;
	Ar << Width;
	Ar << Height;
}

void UBillboardComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	// 텍스처 SRV 재바인딩
	SetTexture(TextureName);
}

void UBillboardComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = FResourceManager::Get().FindTexture(InTextureName);
	// 텍스처 유무가 batcher/Primitive 경로 분기를 좌우하므로 Mesh 단계까지 재갱신 필요.
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UBillboardComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::Name, &TextureName });
	OutProps.push_back({ "Width",  EPropertyType::Float, &Width,  0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Ignore Owner Scale", EPropertyType::Bool, &bIgnoreOwnerScale });
}

void UBillboardComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0)
	{
		SetTexture(TextureName);
	}
	else if (strcmp(PropertyName, "Width") == 0 || strcmp(PropertyName, "Height") == 0 ||
			 strcmp(PropertyName, "Ignore Owner Scale") == 0)
	{
		// Width/Height는 빌보드 쿼드 크기를 결정하므로 트랜스폼/월드 바운드 모두 갱신해야 한다.
		MarkProxyDirty(EDirtyFlag::Transform);
		MarkWorldBoundsDirty();
	}
}

bool UBillboardComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!IsHitTestEnabled())
	{
		return false;
	}

	FMatrix PerRayBillboard = ComputeBillboardMatrix(Ray.Direction);
	FVector BillboardScale = GetBillboardScale();
	FMatrix HitWorldMatrix = FMatrix::MakeScaleMatrix(FVector(BillboardScale.X, Width * BillboardScale.Y, Height * BillboardScale.Z)) * PerRayBillboard;
	FMatrix InvWorldMatrix = HitWorldMatrix.GetInverse();

	FRay LocalRay;
	LocalRay.Origin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
	LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();

	if (std::abs(LocalRay.Direction.X) < 0.00111f) return false;

	float t = -LocalRay.Origin.X / LocalRay.Direction.X;
	if (t < 0.0f) return false;

	FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * t;
	if (LocalHitPos.Y >= -0.5f && LocalHitPos.Y <= 0.5f &&
		LocalHitPos.Z >= -0.5f && LocalHitPos.Z <= 0.5f)
	{
		FVector WorldHitPos = HitWorldMatrix.TransformPositionWithW(LocalHitPos);
		OutHitResult.Distance = (WorldHitPos - Ray.Origin).Length();
		OutHitResult.HitComponent = this;
		return true;
	}

	return false;
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

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetBillboardScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

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

	return RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}

FVector UBillboardComponent::GetBillboardScale() const
{
	if (bIgnoreOwnerScale)
	{
		return FVector(1.0f, 1.0f, 1.0f);
	}

	return GetWorldScale();
}
