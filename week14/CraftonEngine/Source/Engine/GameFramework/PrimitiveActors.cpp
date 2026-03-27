#include "GameFramework/PrimitiveActors.h"
#include "Object/ObjectFactory.h"

#include "Component/PrimitiveComponent.h"
#include "Component/TextRenderComponent.h"
#include <format>
#include <Component/SubUVComponent.h>
#include "Mesh/ObjManager.h"

// 나중에 PrimtiveComponent.h에 있는 Cube, Sphere, Plane 지우기
#include "Component/StaticMeshComponent.h"

IMPLEMENT_CLASS(ACubeActor, AActor)
IMPLEMENT_CLASS(ASphereActor, AActor)
IMPLEMENT_CLASS(APlaneActor, AActor)
IMPLEMENT_CLASS(AStaticMeshActor, AActor)
IMPLEMENT_CLASS(AAttachTestActor, AActor)

void ACubeActor::InitDefaultComponents()
{
	auto* Cube = AddComponent<UCubeComponent>();
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
	auto* Sphere = AddComponent<USphereComponent>();
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
	auto* Plane = AddComponent<UPlaneComponent>();
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

void AStaticMeshActor::InitDefaultComponents()
{
	auto* SM = AddComponent<UStaticMeshComp>();
	SetRootComponent(SM);

	// 큐브 에셋을 매니저에게 요청 (없으면 파싱해서 만들고, 있으면 캐시된 걸 줌)
	UStaticMesh* CubeAsset = FObjManager::LoadObjStaticMesh("Data/cube-tex.obj");

	// 컴포넌트에 에셋 할당 (Assign)
	SM->SetStaticMesh(CubeAsset);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->AttachToComponent(SM);
	Text->SetText("UUID : " + std::to_string(GetUUID()));
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}

void AAttachTestActor::InitDefaultComponents()
{
	// Root: Cube
	auto* Cube = AddComponent<UCubeComponent>();
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
		auto* Sphere = AddComponent<USphereComponent>();
		Sphere->AttachToComponent(Primitives);
		Sphere->SetRelativeLocation(Positions[i]);
	}

	// Text attached directly to Root
	auto* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(Cube);
	Text->SetText("UUID : " + std::to_string(GetUUID()));
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
}
