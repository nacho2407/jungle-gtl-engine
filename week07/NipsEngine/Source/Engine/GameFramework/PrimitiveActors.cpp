#include "GameFramework/PrimitiveActors.h"

#include "Component/FireballComponent.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
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

DEFINE_CLASS(ASceneActor, AActor) 
REGISTER_FACTORY(ASceneActor)

DEFINE_CLASS(AStaticMeshActor, AActor) 
REGISTER_FACTORY(AStaticMeshActor)

DEFINE_CLASS(ASubUVActor, AActor) 
REGISTER_FACTORY(ASubUVActor)

DEFINE_CLASS(ATextRenderActor, AActor) 
REGISTER_FACTORY(ATextRenderActor)

DEFINE_CLASS(ABillboardActor, AActor) 
REGISTER_FACTORY(ABillboardActor)

DEFINE_CLASS(ADecalActor, AActor)
REGISTER_FACTORY(ADecalActor)

DEFINE_CLASS(AFireballActor, AActor)
REGISTER_FACTORY(AFireballActor)

DEFINE_CLASS(ASpotLightActor, AActor)
REGISTER_FACTORY(ASpotLightActor)

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
	Text->SetTransient(true);
	Text->SetEditorOnly(true);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
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
	Text->SetTransient(true);
	Text->SetEditorOnly(true);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}

void APlaneActor::InitDefaultComponents()
{
	auto* Plane = AddComponent<UStaticMeshComponent>();
	Plane->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(PlaneMeshPath));
	SetRootComponent(Plane);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->SetText(std::format("UUID: {}", GetUUID()));
	Text->SetTransient(true);
	Text->SetEditorOnly(true);
	Text->AttachToComponent(Plane);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
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
	Text->SetTransient(true);
	Text->SetEditorOnly(true);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
}

void ASceneActor::InitDefaultComponents()
{
	auto SceneRoot = AddComponent<USceneComponent>();
	SetRootComponent(SceneRoot);
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
	Text->SetTransient(true);
	Text->SetEditorOnly(true);

	FVector Extent = StaticMesh->GetWorldAABB().GetExtent();
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Z * 2.0f));
}

void ASubUVActor::InitDefaultComponents()
{
	SetTickInEditor(true); // Editor Tick을 받도록 변경

    auto* SubUV = AddComponent<USubUVComponent>();
    SetRootComponent(SubUV);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);
    
    auto* Text = AddComponent<UTextRenderComponent>();
    Text->AttachToComponent(SubUV);
    Text->SetFont(FName("Default"));
    Text->SetText("UUID: " + std::to_string(GetUUID()));
	Text->SetTransient(true);
	Text->SetEditorOnly(true);

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
	TextUUID->SetTransient(true);
	TextUUID->SetEditorOnly(true);

    FVector Extent = TextUUID->GetWorldAABB().GetExtent();
    TextUUID->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 0.6f));
}

void ABillboardActor::InitDefaultComponents()
{	
	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	SetRootComponent(Billboard);
	Billboard->SetTextureName(("Asset\\Texture\\Pawn_64x.png"));
	//Billboard->SetTextureName();

    auto* TextUUID = AddComponent<UTextRenderComponent>();
    TextUUID->AttachToComponent(Billboard);
    TextUUID->SetFont(FName("Default"));
    TextUUID->SetText("UUID: " + std::to_string(GetUUID()));
	TextUUID->SetTransient(true);
	TextUUID->SetEditorOnly(true);

    FVector Extent = TextUUID->GetWorldAABB().GetExtent();
    TextUUID->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 0.6f));
}

void ADecalActor::InitDefaultComponents()
{
	UDecalComponent* Decal = AddComponent<UDecalComponent>();
	SetRootComponent(Decal);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(Decal);
	Billboard->SetTextureName(("Asset\\Texture\\DecalActor_64.png"));

	auto* TextUUID = AddComponent<UTextRenderComponent>();
	TextUUID->AttachToComponent(Decal);
	TextUUID->SetFont(FName("Default"));
	TextUUID->SetText("UUID: " + std::to_string(GetUUID()));
	TextUUID->SetTransient(true);
	TextUUID->SetEditorOnly(true);
	FVector Extent = TextUUID->GetWorldAABB().GetExtent();
	TextUUID->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 0.6f));
}

void AFireballActor::InitDefaultComponents()
{
	// Base for debugging and demonstration. Remove this later
    auto* Sphere = AddComponent<UStaticMeshComponent>();
    Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(SphereMeshPath));
	Sphere->SetEnableCull(false);
    SetRootComponent(Sphere);

	// Nametag
    UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
    Text->SetFont(FName("Default"));
    Text->AttachToComponent(Sphere);
    Text->SetText("UUID: " + std::to_string(GetUUID()));
    Text->SetTransient(true);
    Text->SetEditorOnly(true);
    Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));

	// Flare
    UFireballComponent* Fireball = AddComponent<UFireballComponent>();
	Fireball->AttachToComponent(Sphere);

	// Emissive glow material for the fireball core
	//static FMaterial FireballCoreMaterial;
	//static bool bFireballMatInit = false;
	//if (!bFireballMatInit)
	//{
	//	FColor LightColor = Fireball->GetLinearColor();
	//	FVector SurfaceColor = FVector(LightColor.R, LightColor.G, LightColor.B);
	//	FireballCoreMaterial.EmissiveColor = SurfaceColor;
	//	FireballCoreMaterial.DiffuseColor  = SurfaceColor;
	//	bFireballMatInit = true;
	//}
	//Sphere->SetMaterial(0, &FireballCoreMaterial);
}

void ASpotLightActor::InitDefaultComponents() {
	UBillboardComponent* BillboardIcon = AddComponent<UBillboardComponent>();
    BillboardIcon->SetTextureName(("Asset\\Texture\\SpotLight_64x.png"));
	SetRootComponent(BillboardIcon);

	UDecalComponent* Decal = AddComponent<UDecalComponent>();
	Decal->AttachToComponent(BillboardIcon);
	Decal->SetRelativeLocation(FVector(10, 0, 0));
	DecalComp = Decal;

	UMaterial* DecalMat = FResourceManager::Get().GetMaterial("DecalMat");
	UMaterialInstance* DecalMatInst = UMaterialInstance::Create(DecalMat);
	Decal->SetMaterial(DecalMatInst);
	DecalMatInst->SetTexture("DiffuseMap", FResourceManager::Get().LoadTexture("Asset/Texture/DecalFakeSpotlight.png"));
}

void ASpotLightActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	if (DecalComp)
	{
		DecalComp->SetSize(FVector(Range, Range, Range));
	}
}
