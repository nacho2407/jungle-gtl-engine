#include "SubUVComponent.h"

#include <cstring>
#include "Render/Mesh/MeshManager.h"
#include "Core/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"

DEFINE_CLASS(USubUVComponent, UPrimitiveComponent)
REGISTER_FACTORY(USubUVComponent)

USubUVComponent::USubUVComponent()
{
	MeshData = &FMeshManager::Get().GetQuad();
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
	OutProps.push_back({ "Frame Rate", EPropertyType::Float, &FrameRate, 1.0f, 120.0f, 1.0f });
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
	FVector LExt = { 0.01f, 0.5f, 0.5f };

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

	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

void USubUVComponent::TickComponent(float DeltaTime)
{
	if (!CachedParticle) return;

	const uint32 TotalFrames = CachedParticle->Columns * CachedParticle->Rows;
	if (TotalFrames == 0) return;

	TimeAccumulator += DeltaTime;
	const float FrameDuration = 1.0f / FrameRate;
	while (TimeAccumulator >= FrameDuration)
	{
		TimeAccumulator -= FrameDuration;
		FrameIndex = (FrameIndex + 1) % TotalFrames;
	}

	FVector WorldLocation = GetWorldLocation();

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();

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

