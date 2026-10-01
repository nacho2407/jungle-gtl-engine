// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "Component/CylindricalBillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Scene/Proxies/Primitive/CylindricalBillboardSceneProxy.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"

#include <cmath>

IMPLEMENT_COMPONENT_CLASS(UCylindricalBillboardComponent, UBillboardComponent, EEditorComponentCategory::Basic)

FPrimitiveProxy* UCylindricalBillboardComponent::CreateSceneProxy()
{
    return new FCylindricalBillboardSceneProxy(this);
}

void UCylindricalBillboardComponent::Serialize(FArchive& Ar)
{
    UBillboardComponent::Serialize(Ar);
    Ar << BillboardAxis;
}

void UCylindricalBillboardComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UBillboardComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "BillboardAxis", EPropertyType::Vec3, &BillboardAxis });
}

void UCylindricalBillboardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    if (!GetOwner() || !GetOwner()->GetWorld())
        return;

    const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
    if (!ActiveCamera)
        return;

    CachedWorldMatrix = ComputeBillboardMatrix(ActiveCamera->GetForwardVector());
    UpdateWorldAABB();
}

FMatrix UCylindricalBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
    FVector BillboardPos = GetWorldLocation();
    FVector BillboardForward = CameraForward * -1.0f;

    FVector LocalAxis = BillboardAxis;
    if (LocalAxis.Dot(LocalAxis) < 0.0001f)
    {
        LocalAxis = FVector(0, 0, 1);
    }
    else
    {
        LocalAxis.Normalize();
    }

    FMatrix NonBillboardWorldMatrix;
    if (GetParent())
    {
        NonBillboardWorldMatrix = GetRelativeMatrix() * GetParent()->GetWorldMatrix();
    }
    else
    {
        NonBillboardWorldMatrix = GetRelativeMatrix();
    }

    FVector WorldAxis = NonBillboardWorldMatrix.TransformVector(LocalAxis).Normalized();

    // 카메라 방향을 축에 수직인 평면에 투영
    FVector Forward = BillboardForward - (WorldAxis * BillboardForward.Dot(WorldAxis));
    if (Forward.Dot(Forward) < 0.0001f)
    {
        FVector TempUp = (std::abs(WorldAxis.Dot(FVector(0, 0, 1))) < 0.99f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
        Forward = TempUp.Cross(WorldAxis).Normalized();
    }
    else
    {
        Forward.Normalize();
    }

    FVector Right = WorldAxis.Cross(Forward).Normalized();
    FVector Up = WorldAxis;

    FMatrix RotMatrix;
    RotMatrix.SetAxes(Right, Up, Forward);

    return FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(BillboardPos);
}

