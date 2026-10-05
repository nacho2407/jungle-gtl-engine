#include "SubUVComponent.h"

#include <cmath>
#include <cstring>
#include "Camera/ViewportCamera.h"
#include "Core/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Math/Utils.h"
#include "Render/Scene/Scene.h"

USubUVComponent::USubUVComponent()
{
	SetVisibility(true);
	SetInheritOwnerScale(true);
}

// 재생 상태 등 reflection에 노출되지 않은 필드를 직접 복사합니다.
// CachedSubUV 은 CopyPropertiesFrom 내부에서 SubUV(Name) 처리 시
// PostEditProperty("SubUV") → SetSubUV() 를 통해 자동으로 갱신됩니다.
void USubUVComponent::PostDuplicate(UObject* Original)
{
	UBillboardComponent::PostDuplicate(Original);

	const USubUVComponent* Orig = Cast<USubUVComponent>(Original);
	// 현재 프레임/누적 시간을 유지해 PIE 진입 시 에디터에서 보던 파티클 상태를 이어 재생합니다.
	bIsExecute = Orig->bIsExecute;
}

void USubUVComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);

	if (Ar.IsLoading())
	{
		SetSubUV(SubUVName);
	}
}

void USubUVComponent::SetSubUV(const FName& InSubUVName)
{
	SubUVName = InSubUVName;
	CachedSubUV = FResourceManager::Get().FindSubUV(InSubUVName);
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::Resource);
}

void USubUVComponent::SetSpriteSize(float InWidth, float InHeight)
{
	Width = InWidth;
	Height = InHeight;
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::Mesh);
}

const FSubUVResource* USubUVComponent::GetSubUV() const
{
	return FResourceManager::Get().FindSubUV(SubUVName);
}

void USubUVComponent::PostEditProperty(const char* PropertyName)
{
	UBillboardComponent::PostEditProperty(PropertyName);

	if (PropertyName && strcmp(PropertyName, "SubUVName") == 0)
	{
		SetSubUV(SubUVName);
	}
}

void USubUVComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	const FViewportCamera* Camera = nullptr;

	if (TryGetActiveCamera(Camera) && Camera != nullptr)
	{
		CachedWorldMatrix = MakeBillboardWorldMatrix(GetWorldLocation(),
			GetBillboardWorldScale(),
			Camera->GetEffectiveForward(),
			Camera->GetEffectiveRight(),
			Camera->GetEffectiveUp());
	}
	else
	{
		// 카메라를 찾을 수 없는 로드 초기 시점 등에서는 기본 축을 사용합니다.
		CachedWorldMatrix = MakeBillboardWorldMatrix(GetWorldLocation(),
			GetBillboardWorldScale(),
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

bool USubUVComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	FMatrix BillboardWorldMatrix = MakeBillboardWorldMatrix(
		GetWorldLocation(),
		GetBillboardWorldScale(),
		FVector(1.0f, 0.0f, 0.0f),
		FVector(0.0f, 1.0f, 0.0f),
		FVector(0.0f, 0.0f, 1.0f));
	const FViewportCamera* ActiveCamera = nullptr;
	if (TryGetActiveCamera(ActiveCamera))
	{
		BillboardWorldMatrix = MakeBillboardWorldMatrix(
			GetWorldLocation(),
			GetBillboardWorldScale(),
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

void USubUVComponent::TickComponent(float DeltaTime)
{
	UBillboardComponent::TickComponent(DeltaTime);

	const FSubUVResource* SubUV = GetSubUV();
	if (!SubUV) return;
	if (!bLoop && bIsExecute) return; // 단발 재생 완료 후 정지

	const uint32 TotalFrames = SubUV->Columns * SubUV->Rows;
	if (TotalFrames == 0) return;

	TimeAccumulator += DeltaTime;
	const float FrameDuration = 1.0f / PlayRate;
	while (TimeAccumulator >= FrameDuration)
	{
		TimeAccumulator -= FrameDuration;

		if (bLoop)
		{
			bIsExecute = false;
			FrameIndex = (FrameIndex + 1) % TotalFrames; // 무한 반복
		}
		else
		{
			if (FrameIndex < TotalFrames - 1)
			{
				FrameIndex++;
			}
			else
			{
				bIsExecute = true;    // 마지막 프레임 도달 → 완료
				TimeAccumulator = 0.0f;
				break;
			}
		}
	}
}

