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

DEFINE_CLASS(AStaticMeshActor, AActor) 
REGISTER_FACTORY(AStaticMeshActor)

DEFINE_CLASS(ASubUVActor, AActor) 
REGISTER_FACTORY(ASubUVActor)

DEFINE_CLASS(ATextRenderActor, AActor) 
REGISTER_FACTORY(ATextRenderActor)

void ACubeActor::InitDefaultComponents()
{
	auto* Cube = AddComponent<UStaticMeshComponent>();
	Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
	SetRootComponent(Cube);

	// Text
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->AttachToComponent(Cube);
	Text->SetText("UUID: " + std::to_string(GetUUID()));
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
	Text->SetText("UUID: " + std::to_string(GetUUID()));
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
	Text->SetText("UUID: " + std::to_string(GetUUID()));
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
}

void AStaticMeshActor::InitDefaultComponents()
{
	auto* StaticMesh = AddComponent<UStaticMeshComponent>();;
	SetRootComponent(StaticMesh);

	// Text attached directly to Root
	auto* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(StaticMesh);
	Text->SetFont(FName("Default"));
	Text->SetText("UUID: " + std::to_string(GetUUID()));

	FVector Extent = StaticMesh->GetWorldAABB().GetExtent();
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Z * 2.0f));
}

void ASubUVActor::InitDefaultComponents()
{
    auto* SubUV = AddComponent<USubUVComponent>();
    SetRootComponent(SubUV);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);
    
    auto* Text = AddComponent<UTextRenderComponent>();
    Text->AttachToComponent(SubUV);
    Text->SetFont(FName("Default"));
    Text->SetText("UUID: " + std::to_string(GetUUID()));

    FVector Extent = SubUV->GetWorldAABB().GetExtent();
    Text->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 1.4f));
}

void ATextRenderActor::InitDefaultComponents()
{
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	SetRootComponent(Text);
	Text->SetFont(FName("Default"));
	Text->SetText("TextRender");
    
    auto* TextUUID = AddComponent<UTextRenderComponent>();
    TextUUID->AttachToComponent(Text);
    TextUUID->SetFont(FName("Default"));
    TextUUID->SetText("UUID: " + std::to_string(GetUUID()));

    FVector Extent = TextUUID->GetWorldAABB().GetExtent();
    TextUUID->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 0.6f));
}