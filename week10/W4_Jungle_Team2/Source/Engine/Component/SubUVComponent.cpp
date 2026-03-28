#include "SubUVComponent.h"

#include <cstring>
#include "Core/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Math/Utils.h"

DEFINE_CLASS(USubUVComponent, UBillboardComponent)
REGISTER_FACTORY(USubUVComponent)

USubUVComponent::USubUVComponent()
{
	SetVisibility(true);
}

void USubUVComponent::SetParticle(const FName& InParticleName)
{
	ParticleName = InParticleName;
	CachedParticle = FResourceManager::Get().FindParticle(InParticleName);
}

void USubUVComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Particle", EPropertyType::Name, &ParticleName });
	OutProps.push_back({ "Width", EPropertyType::Float, &Width, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Play Rate", EPropertyType::Float, &PlayRate, 1.0f, 120.0f, 1.0f });
	OutProps.push_back({ "bLoop", EPropertyType::Bool, &bLoop });
}

void USubUVComponent::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "Particle") == 0)
	{
		SetParticle(ParticleName);
	}
}

void USubUVComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

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
	const FMatrix InvWorld = GetWorldMatrix().GetInverse();

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

	const FVector HitWorld = GetWorldMatrix().TransformPosition(HitLocal);

	OutHitResult.bHit = true;
	OutHitResult.HitComponent = this;
	OutHitResult.Distance = FVector::Distance(Ray.Origin, HitWorld);
	OutHitResult.Location = HitWorld;
	OutHitResult.Normal = GetForwardVector();
	OutHitResult.FaceIndex = 0;
	return true;
}

void USubUVComponent::TickComponent(float DeltaTime)
{
	UBillboardComponent::TickComponent(DeltaTime);

	if (!CachedParticle) return;
	if (!bLoop && bIsExecute) return; // 단발 재생 완료 후 정지

	const uint32 TotalFrames = CachedParticle->Columns * CachedParticle->Rows;
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

