#include "GameFramework/PrimitiveActors.h"

#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Core/ResourceManager.h"
#include <format>
#include <Component/SubUVComponent.h>

namespace
{
	constexpr const char* CubeMeshPath = "Asset/Mesh/Cube.obj";
	constexpr const char* SphereMeshPath = "Asset/Mesh/Sphere.obj";
	constexpr const char* PlaneMeshPath = "Asset/Mesh/Plane.obj";
}

DEFINE_CLASS(ACubeActor, AActor)
REGISTER_FACTORY(ACubeActor)

DEFINE_CLASS(ASphereActor, AActor)
REGISTER_FACTORY(ASphereActor)

DEFINE_CLASS(APlaneActor, AActor)
REGISTER_FACTORY(APlaneActor)

DEFINE_CLASS(AAttachTestActor, AActor)
REGISTER_FACTORY(AAttachTestActor)

void ACubeActor::InitDefaultComponents()
{
	auto* Cube = AddComponent<UStaticMeshComponent>();
	Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
	SetRootComponent(Cube);

	// Text
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->AttachToComponent(Cube);
	Text->SetText("UUID : " + std::to_string(GetUUID()));
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));

	// SubUV
	USubUVComponent* SubUV = AddComponent<USubUVComponent>();
	SubUV->AttachToComponent(Cube);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);
	SubUV->SetRelativeLocation(FVector(0.0f, 0.0f, 2.3f));
}

void ASphereActor::InitDefaultComponents()
{
	auto* Sphere = AddComponent<UStaticMeshComponent>();
	Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(SphereMeshPath));
	SetRootComponent(Sphere);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->AttachToComponent(Sphere);
	Text->SetText("UUID : " + std::to_string(GetUUID()));
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));

	// SubUV
	USubUVComponent* SubUV = AddComponent<USubUVComponent>();
	SubUV->AttachToComponent(Sphere);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);
	SubUV->SetRelativeLocation(FVector(0.0f, 0.0f, 2.3f));
}

void APlaneActor::InitDefaultComponents()
{
	auto* Plane = AddComponent<UStaticMeshComponent>();
	Plane->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(PlaneMeshPath));
	SetRootComponent(Plane);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->SetText(std::format("UUID: {}", GetUUID()));
	Text->AttachToComponent(Plane);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));

	// SubUV
	USubUVComponent* SubUV = AddComponent<USubUVComponent>();
	SubUV->AttachToComponent(Plane);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);
	SubUV->SetRelativeLocation(FVector(0.0f, 0.0f, 2.3f));
}

void AAttachTestActor::InitDefaultComponents()
{
	// Root: Cube
	auto* Cube = AddComponent<UStaticMeshComponent>();
	Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
	SetRootComponent(Cube);

	// Grouping node for spheres
	auto* Primitives = AddComponent<USceneComponent>();
	Primitives->AttachToComponent(Cube);

	// 4 Spheres in a square pattern
	constexpr float Offset = 2.0f;
	const FVector Positions[4] = {
		{ -Offset, -Offset, 0.0f },
		{  Offset, -Offset, 0.0f },
		{  Offset,  Offset, 0.0f },
		{ -Offset,  Offset, 0.0f },
	};
	for (int i = 0; i < 4; ++i)
	{
		auto* Sphere = AddComponent<UStaticMeshComponent>();
		Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(SphereMeshPath));
		Sphere->AttachToComponent(Primitives);
		Sphere->SetRelativeLocation(Positions[i]);
	}

	// Text attached directly to Root
	auto* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(Cube);
	Text->SetText("UUID : " + std::to_string(GetUUID()));
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
}
