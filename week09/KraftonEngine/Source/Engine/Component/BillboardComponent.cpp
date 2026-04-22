#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <cstring>
#include <algorithm>

namespace
{
FMatrix BuildStableBillboardMatrix(
    const FVector& CameraForward,
    const FVector& WorldLocation,
    const FVector& WorldScale,
    float Width,
    float Height)
{
    // billboard normal: 카메라를 향하도록
    FVector Forward = (CameraForward * (-1)).Normalized();

    // z-up 고정
    FVector WorldUp(0.0f, 0.0f, 1.0f);
    if (std::abs(Forward.Dot(WorldUp)) > 0.95f)
    {
        WorldUp = FVector(0.0f, 1.0f, 0.0f);
    }

    // local Y -> world Right
    FVector Right = WorldUp.Cross(Forward).Normalized();

    // local Z -> world Up
    FVector Up = Forward.Cross(Right).Normalized();

    FMatrix RotMatrix;

    // local X=depth, Y=width, Z=height 라는 현재 scale/AABB 가정에 맞춤
    RotMatrix.SetAxes(Forward, Right, Up);

    const FVector SpriteScale(
        std::max(std::abs(WorldScale.X), 0.1f), // depth
        Width * std::abs(WorldScale.Y),         // width
        Height * std::abs(WorldScale.Z));       // height

    return FMatrix::MakeScaleMatrix(SpriteScale) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);
}
} // namespace

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
    OutProps.push_back({ "Width", EPropertyType::Float, &Width, 0.1f, 100.0f, 0.1f });
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
    if (!GetOwner() || !GetOwner()->GetWorld())
        return;

    const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
    if (!ActiveCamera)
        return;

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
    RotMatrix.SetAxes(Right, Up, Forward);

    CachedWorldMatrix = ComputeBillboardMatrix(CameraForward);

    UpdateWorldAABB();
}

FMatrix UBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
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

	const FVector WorldScale = GetWorldScale();
	const FVector SpriteScale(
		std::max(WorldScale.X, 1.0f),
		Width * WorldScale.Y,
		Height * WorldScale.Z);

	return FMatrix::MakeScaleMatrix(SpriteScale) * RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}

void UBillboardComponent::UpdateWorldAABB() const
{
    const FVector WorldScale = GetWorldScale();
    const float HalfWidth = 0.5f * Width * std::abs(WorldScale.Y);
    const float HalfHeight = 0.5f * Height * std::abs(WorldScale.Z);
    const float HalfDepth = 0.5f * std::max(std::abs(WorldScale.X), 0.1f);

    const FVector Center = GetWorldLocation();
    const FVector Extent(HalfDepth, HalfWidth, HalfHeight);
    WorldAABBMinLocation = Center - Extent;
    WorldAABBMaxLocation = Center + Extent;
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}

bool UBillboardComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
    FVector CameraForward = Ray.Direction;
    if (const AActor* OwnerActor = GetOwner())
    {
        if (const UWorld* World = OwnerActor->GetWorld())
        {
            if (const UCameraComponent* ActiveCamera = World->GetActiveCamera())
            {
                CameraForward = ActiveCamera->GetForwardVector();
            }
        }
    }

    const FMatrix WorldMatrix = bIsBillboard ? ComputeBillboardMatrix(CameraForward) : GetWorldMatrix();
    const FMatrix InvWorldMatrix = WorldMatrix.GetInverse();

    FRay LocalRay;
    LocalRay.Origin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
    LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();

    if (std::abs(LocalRay.Direction.X) < 0.001f)
    {
        return false;
    }

    const float T = -LocalRay.Origin.X / LocalRay.Direction.X;
    if (T < 0.0f)
    {
        return false;
    }

    const FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * T;
    if (LocalHitPos.Y < -0.5f || LocalHitPos.Y > 0.5f ||
        LocalHitPos.Z < -0.5f || LocalHitPos.Z > 0.5f)
    {
        return false;
    }

    const FVector WorldHitPos = WorldMatrix.TransformPositionWithW(LocalHitPos);
    OutHitResult.Distance = (WorldHitPos - Ray.Origin).Length();
    OutHitResult.HitComponent = this;
    return true;
}
