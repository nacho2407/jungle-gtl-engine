#include "BillboardComponent.h"
#include <cmath>
#include "GameFramework/World.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Core/ResourceManager.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)
REGISTER_FACTORY(UBillboardComponent)

// GetEditableProperties 에 노출되지 않은 필드를 직접 복사합니다.
void UBillboardComponent::PostDuplicate(UObject* Original)
{
    UPrimitiveComponent::PostDuplicate(Original);

    const UBillboardComponent* Orig = Cast<UBillboardComponent>(Original);
    bIsBillboard = Orig->bIsBillboard;
    Texture = Orig->Texture; // 얕은 복사 (ResourceManager 소유)
    FrameIndex = Orig->FrameIndex;
    TimeAccumulator = Orig->TimeAccumulator;
}

bool UBillboardComponent::TryGetActiveCamera(const FViewportCamera*& OutCamera) const
{
    OutCamera = nullptr;

    if (GetOwner() == nullptr || GetOwner()->GetWorld() == nullptr)
    {
        return false;
    }

    OutCamera = GetOwner()->GetWorld()->GetActiveCamera();
    return OutCamera != nullptr;
}

// 카메라 Forward, Right, Up Vector 기반으로 billboard 의 world 행렬 생성
FMatrix UBillboardComponent::MakeBillboardWorldMatrix(
    const FVector& WorldLocation,
    const FVector& WorldScale,
    const FVector& CameraForward,
    const FVector& CameraRight,
    const FVector& CameraUp)
{
    FVector Forward = CameraForward.GetSafeNormal();
    FVector Right = (-CameraRight).GetSafeNormal();
    FVector Up = CameraUp.GetSafeNormal();

    if (Forward.IsNearlyZero())
    {
        Forward = FVector(-1.0f, 0.0f, 0.0f);
    }

    if (Right.IsNearlyZero() || Up.IsNearlyZero())
    {
        FVector FallbackUp = FVector::UpVector;
        if (std::abs(FVector::DotProduct(Forward, FallbackUp)) > 0.99f)
        {
            FallbackUp = FVector::RightVector;
        }

        Right = FVector::CrossProduct(FallbackUp, Forward).GetSafeNormal();
        Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
    }

    FMatrix BillboardMatrix = FMatrix::Identity;
    BillboardMatrix.SetAxes(
        Forward * WorldScale.X,
        Right * WorldScale.Y,
        Up * WorldScale.Z,
        WorldLocation);
    return BillboardMatrix;
}

void UBillboardComponent::SetTextureName(FString InName)
{
    TextureName = InName;
    Texture = FResourceManager::Get().GetTexture(InName);
}

FString UBillboardComponent::GetTextureName()
{
    return TextureName.ToString();
}

UTexture* UBillboardComponent::GetTexture()
{
    if (Texture == nullptr)
    {
        Texture = FResourceManager::Get().GetTexture(TextureName.ToString());
    }
    return Texture;
}

void UBillboardComponent::UpdateWorldAABB() const

{
    WorldAABB.Reset();

    const FViewportCamera* Camera = nullptr;

    if (TryGetActiveCamera(Camera) && Camera != nullptr)
    {
        CachedWorldMatrix = MakeBillboardWorldMatrix(GetWorldLocation(),
                                                     GetWorldScale(),
                                                     Camera->GetEffectiveForward(),
                                                     Camera->GetEffectiveRight(),
                                                     Camera->GetEffectiveUp());
    }
    else
    {
        // 카메라를 찾을 수 없는 로드 초기 시점 등에서는 기본 축을 사용합니다.
        CachedWorldMatrix = MakeBillboardWorldMatrix(GetWorldLocation(),
                                                     GetWorldScale(),
                                                     FVector(1.0f, 0.0f, 0.0f),  // Forward
                                                     FVector(0.0f, 1.0f, 0.0f),  // Right
                                                     FVector(0.0f, 0.0f, 1.0f)); // Up
    }

    FVector LExt = { 0.01f, Width * 0.5f, Height * 0.5f };

    float NewEx = std::abs(CachedWorldMatrix.M[0][0]) * LExt.X +
                  std::abs(CachedWorldMatrix.M[1][0]) * LExt.Y +
                  std::abs(CachedWorldMatrix.M[2][0]) * LExt.Z;

    float NewEy = std::abs(CachedWorldMatrix.M[0][1]) * LExt.X +
                  std::abs(CachedWorldMatrix.M[1][1]) * LExt.Y +
                  std::abs(CachedWorldMatrix.M[2][1]) * LExt.Z;

    float NewEz = std::abs(CachedWorldMatrix.M[0][2]) * LExt.X +
                  std::abs(CachedWorldMatrix.M[1][2]) * LExt.Y +
                  std::abs(CachedWorldMatrix.M[2][2]) * LExt.Z;

    FVector WorldCenter = GetWorldLocation();
    const FVector Min = WorldCenter - FVector(NewEx, NewEy, NewEz);
    const FVector Max = WorldCenter + FVector(NewEx, NewEy, NewEz);

    WorldAABB.Expand(Min);
    WorldAABB.Expand(Max);
}


bool UBillboardComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)

{
    FMatrix BillboardWorldMatrix = GetWorldMatrix();
    const FViewportCamera* ActiveCamera = nullptr;
    if (TryGetActiveCamera(ActiveCamera))
    {
        BillboardWorldMatrix = MakeBillboardWorldMatrix(
            GetWorldLocation(),
            GetWorldScale(),
            ActiveCamera->GetEffectiveForward(),
            ActiveCamera->GetEffectiveRight(),
            ActiveCamera->GetEffectiveUp());
    }

    const FMatrix InvWorld = BillboardWorldMatrix.GetInverse();

    FRay LocalRay;
    LocalRay.Origin = InvWorld.TransformPosition(Ray.Origin);
    LocalRay.Direction = InvWorld.TransformVector(Ray.Direction);
    LocalRay.Direction.NormalizeSafe();

    if (std::abs(LocalRay.Direction.X) < MathUtil::Epsilon)
    {
        return false;
    }

    const float T = -LocalRay.Origin.X / LocalRay.Direction.X;
    if (T < 0.0f)
    {
        return false;
    }

    const FVector HitLocal = LocalRay.Origin + LocalRay.Direction * T;
    const float HalfW = Width * 0.5f;
    const float HalfH = Height * 0.5f;

    if (HitLocal.Y < -HalfW || HitLocal.Y > HalfW || HitLocal.Z < -HalfH || HitLocal.Z > HalfH)
    {
        return false;
    }

    const FVector HitWorld = BillboardWorldMatrix.TransformPosition(HitLocal);

    OutHitResult.bHit = true;
    OutHitResult.HitComponent = this;
    OutHitResult.Distance = FVector::Distance(Ray.Origin, HitWorld);
    OutHitResult.Location = HitWorld;
    OutHitResult.Normal = BillboardWorldMatrix.GetForwardVector();
    OutHitResult.FaceIndex = 0;
    return true;
}

void UBillboardComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Particle", EPropertyType::Name, &TextureName });
    OutProps.push_back({ "Width", EPropertyType::Float, &Width, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Play Rate", EPropertyType::Float, &PlayRate, 1.0f, 120.0f, 1.0f });
    OutProps.push_back({ "bLoop", EPropertyType::Bool, &bLoop });
}

void UBillboardComponent::TickComponent(float DeltaTime)
{
    (void)DeltaTime;
    UpdateWorldAABB();
}
