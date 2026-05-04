#include "GameFramework/PrimitiveActors.h"

#include "Component/FireballComponent.h"
#include "Component/DecalComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"

#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"

#include "Component/Movement/RotatingMovementComponent.h"
#include "Core/ResourceManager.h"
#include <format>
#include <Component/SubUVComponent.h>
#include "Core/Debug.h"
#include "Component/BoxComponent.h"
#include "Core/CollisionTypes.h"
#include "Component/ProceduralMeshComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <random>

namespace
{
	constexpr const char* CubeMeshPath = "Asset/Mesh/Cube.obj";
	constexpr const char* FireballMeshPath = "Asset/Mesh/Sun/sun.obj";
	constexpr const char* PlaneMeshPath = "Asset/Mesh/Plane.obj";

    bool GetProceduralMeshLocalCenter(UProceduralMeshComponent* MeshComp, FVector& OutCenter)
    {
        if (!MeshComp)
        {
            return false;
        }

        bool bHasVertex = false;
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const UProceduralMeshComponent::FMeshSection& Section : MeshComp->GetSections())
        {
            for (const FNormalVertex& Vertex : Section.Vertices)
            {
                bHasVertex = true;
                Min.X = std::min(Min.X, Vertex.Position.X);
                Min.Y = std::min(Min.Y, Vertex.Position.Y);
                Min.Z = std::min(Min.Z, Vertex.Position.Z);
                Max.X = std::max(Max.X, Vertex.Position.X);
                Max.Y = std::max(Max.Y, Vertex.Position.Y);
                Max.Z = std::max(Max.Z, Vertex.Position.Z);
            }
        }

        if (!bHasVertex)
        {
            return false;
        }

        OutCenter = (Min + Max) * 0.5f;
        return true;
    }

    std::mt19937& GetMainSceneDestructibleRng()
    {
        static std::mt19937 Rng(std::random_device{}());
        return Rng;
    }

    float RandomRange(float Min, float Max)
    {
        std::uniform_real_distribution<float> Distribution(Min, Max);
        return Distribution(GetMainSceneDestructibleRng());
    }

    FVector RandomUnitVector()
    {
        for (int32 Attempt = 0; Attempt < 8; ++Attempt)
        {
            const FVector Candidate(
                RandomRange(-1.0f, 1.0f),
                RandomRange(-1.0f, 1.0f),
                RandomRange(-1.0f, 1.0f));

            if (Candidate.SizeSquared() > 0.0001f)
            {
                return Candidate.GetSafeNormal();
            }
        }

        return FVector::UpVector;
    }
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

DEFINE_CLASS(ADefaultPlayerActor, AActor)
REGISTER_FACTORY(ADefaultPlayerActor)

DEFINE_CLASS(APlayerStart, AActor)
REGISTER_FACTORY(APlayerStart)

DEFINE_CLASS(AFogActor, AActor)
REGISTER_FACTORY(AFogActor)

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

DEFINE_CLASS(ADecalSpotLightActor, AActor)
REGISTER_FACTORY(ADecalSpotLightActor)

DEFINE_CLASS(ALightActor, AActor)
REGISTER_FACTORY(ALightActor)

DEFINE_CLASS(AAmbientLightActor, ALightActor)
REGISTER_FACTORY(AAmbientLightActor)

DEFINE_CLASS(ADirectionalLightActor, ALightActor)
REGISTER_FACTORY(ADirectionalLightActor)

DEFINE_CLASS(APointLightActor, ALightActor)
REGISTER_FACTORY(APointLightActor)

DEFINE_CLASS(ASpotlightActor, APointLightActor)
REGISTER_FACTORY(ASpotlightActor)

DEFINE_CLASS(ABullet, AActor)
REGISTER_FACTORY(ABullet)

DEFINE_CLASS(ADestructibleActor, AActor)
REGISTER_FACTORY(ADestructibleActor)

DEFINE_CLASS(UMainSceneDestructibleComponent, UActorComponent)
REGISTER_FACTORY(UMainSceneDestructibleComponent)

DEFINE_CLASS(AMainSceneDestructibleActor, AActor)
REGISTER_FACTORY(AMainSceneDestructibleActor)

DEFINE_CLASS(ABladeSlash, AActor)
REGISTER_FACTORY(ABladeSlash)

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
	//Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(SphereMeshPath));
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
		//Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(SphereMeshPath));
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

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(SceneRoot);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName("Asset/Texture/EmptyActor.png");
}

void ADefaultPlayerActor::InitDefaultComponents()
{
	auto* SceneRoot = AddComponent<USceneComponent>();
	SetRootComponent(SceneRoot);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(SceneRoot);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName("Asset/Texture/Pawn_64x.png");

	auto* BodySection = AddComponent<USceneComponent>();
    BodySection->AttachToComponent(SceneRoot);
    BodySection->SetRelativeScale(FVector(1, 1, 1));
    BodySection->SetRelativeRotation(FVector(0, 15.9, 13.1f));
    BodySection->SetRelativeLocation(FVector(0.2, 0.5, 1.3));

	auto* KatanaComp = AddComponent<UStaticMeshComponent>();
    KatanaComp->AttachToComponent(BodySection);
    KatanaComp->SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/cyberpunk_katana/katana.obj"));
    KatanaComp->SetRelativeScale(FVector(3, 3, 1));
    KatanaComp->SetRelativeRotation(FVector(180, 0, -103.3f));
    KatanaComp->SetRelativeLocation(FVector(0.52, -0.8, 0));
    KatanaComp->SetEnableCull(false);

	auto* ArmComp = AddComponent<UStaticMeshComponent>();
    ArmComp->AttachToComponent(BodySection);
    ArmComp->SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/cyberpunk_arm/cyberpunk_arm_right.obj"));
    ArmComp->SetRelativeScale(FVector(1, 1, 1));
    ArmComp->SetRelativeRotation(FVector(82.8f, 17.3f, 53.4f));
    ArmComp->SetRelativeLocation(FVector(0, 0, 0));
    ArmComp->SetEnableCull(false);

	SpringArmComp = AddComponent<USpringArmComponent>();
	SpringArmComp->AttachToComponent(SceneRoot);
	SpringArmComp->SetRelativeLocation(FVector(0.0f, 0.0f, 1.6f));
	SpringArmComp->SetTargetArmLength(0.f);
	SpringArmComp->SetSocketOffset(FVector::ZeroVector);

	CameraComp = AddComponent<UCameraComponent>();
	CameraComp->AttachToComponent(SpringArmComp);
	CameraComp->SetRelativeLocation(SpringArmComp->GetSocketLocalLocation());
	CameraComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
	SpringArmComp->UpdateSocketChildren();
}

void APlayerStart::InitDefaultComponents()
{
	auto* SceneRoot = AddComponent<USceneComponent>();
	SetRootComponent(SceneRoot);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(SceneRoot);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName("Asset/Texture/PlayerStart_64x.PNG");
}

void AFogActor::InitDefaultComponents()
{
	UHeightFogComponent* Fog = AddComponent<UHeightFogComponent>();
	Fog->SetFogDensity(0.02f);
	Fog->SetHeightFalloff(0.2f);
	Fog->SetFogInscatteringColor(FVector4(0.72f, 0.8f, 0.9f, 1.0f));
	Fog->SetFogHeight(0.0f);
	Fog->SetFogStartDistance(0.0f);
	Fog->SetFogCutoffDistance(10000.0f);
	Fog->SetFogMaxOpacity(1.0f);
	SetRootComponent(Fog);
	FogComp = Fog;

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(Fog);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName("Asset/Texture/ExpoHeightFog_64x.png");
	BillboardComp = Billboard;
}

void ASceneActor::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    UE_LOG("%s: On Hit", GetFName().ToString().c_str());
}

void ASceneActor::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG("%s: On Begin Overlap", GetFName().ToString().c_str());
    UE_LOG("Hit: %f %f %f", SweepResult.Location.X, SweepResult.Location.Y, SweepResult.Location.Z);
    UE_LOG("Hit Normal: %f %f %f", SweepResult.Normal.X, SweepResult.Normal.Y, SweepResult.Normal.Z);
}

void ASceneActor::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG("%s: On End Overlap", GetFName().ToString().c_str());
    UE_LOG("Hit: %f %f %f", SweepResult.Location.X, SweepResult.Location.Y, SweepResult.Location.Z);
    UE_LOG("Hit Normal: %f %f %f", SweepResult.Normal.X, SweepResult.Normal.Y, SweepResult.Normal.Z);
}

void AStaticMeshActor::InitDefaultComponents()
{
	auto* StaticMesh = AddComponent<UStaticMeshComponent>();
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
	Billboard->SetTextureName(("Asset/Texture/Pawn_64x.png"));
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
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName(("Asset/Texture/DecalActor_64.png"));

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
    Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(FireballMeshPath));
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
}

void ADecalSpotLightActor::InitDefaultComponents() {
	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
    Billboard->SetTextureName(("Asset/Texture/SpotLight_64x.png"));
	Billboard->SetEditorOnly(true);
	SetRootComponent(Billboard);

	UDecalComponent* Decal = AddComponent<UDecalComponent>();
	Decal->AttachToComponent(Billboard);
	Decal->SetRelativeLocation(FVector(10, 0, 0));
	DecalComp = Decal;

	UMaterialInterface* DecalMat = FResourceManager::Get().GetMaterialInterface("DecalMat_SpotLight");
	if (DecalMat == nullptr)
	{
		UMaterial* DecalOriginMat = FResourceManager::Get().GetMaterial("DecalMat");
		DecalMat = FResourceManager::Get().CreateMaterialInstance(DecalOriginMat->GetFilePath() + "_SpotLight", DecalOriginMat);
	}
	Decal->SetMaterial(DecalMat);
	DecalMat->SetTexture("DiffuseMap", FResourceManager::Get().LoadTexture("Asset/Texture/DecalFakeSpotlight.png"));
}

void ADecalSpotLightActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	if (DecalComp)
	{
		DecalComp->SetSize(FVector(Range, Range, Range));
	}
}

ULightComponent* ALightActor::GetLight() const
{
    if (!LightComp)
        return nullptr;
    return LightComp;
}

void ALightActor::SetLight(ULightComponent* InLight)
{
    if (InLight)
        LightComp = InLight;
}

void AAmbientLightActor::InitDefaultComponents()
{
    UAmbientLightComponent* Ambient = AddComponent<UAmbientLightComponent>();
	Ambient->Intensity = 0.2f;
	SetRootComponent(Ambient);

    UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
    Billboard->SetTextureName("Asset/Texture/SkyLight_64x.png");
    Billboard->AttachToComponent(Ambient);
	Billboard->SetEditorOnly(true);

    SetLight(Ambient);
	SetBillboard(Billboard);
}

void AAmbientLightActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	if (BillboardComp)
	{
		BillboardComp->SetColor(GetLight()->LightColor);
	}
}

void ADirectionalLightActor::InitDefaultComponents()
{
	SetTickInEditor(true);

	UDirectionalLightComponent* Directional = AddComponent<UDirectionalLightComponent>();
	SetRootComponent(Directional);

    UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
    Billboard->SetTextureName(("Asset\\Texture\\DirectionalLight_64x.png"));
	Billboard->AttachToComponent(Directional);
	Billboard->SetEditorOnly(true);

    SetLight(Directional);
	SetBillboard(Billboard);
}

void ADirectionalLightActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	if (BillboardComp)
	{
		BillboardComp->SetColor(GetLight()->LightColor);
	}
}

void APointLightActor::InitDefaultComponents()
{
	SetTickInEditor(true);

	UPointLightComponent* Point = AddComponent<UPointLightComponent>();
	SetRootComponent(Point);

    UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
    Billboard->SetTextureName(("Asset\\Texture\\PointLight_64x.png"));
	Billboard->AttachToComponent(Point);
	Billboard->SetEditorOnly(true);

    SetLight(Point);
	SetBillboard(Billboard);
}

void APointLightActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	if (BillboardComp)
	{
		BillboardComp->SetColor(GetLight()->LightColor);
	}
}

void ASpotlightActor::InitDefaultComponents() {
	SetTickInEditor(true);

	USpotlightComponent* Spot = AddComponent<USpotlightComponent>();
	SetRootComponent(Spot);

    UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
    Billboard->SetTextureName(("Asset\\Texture\\SpotLight_64x.png"));
	Billboard->AttachToComponent(Spot);
	Billboard->SetEditorOnly(true);

	SetLight(Spot);
	SetBillboard(Billboard);
}

void ASpotlightActor::Tick(float DeltaTime) 
{
	AActor::Tick(DeltaTime);

	if (BillboardComp)
	{
		BillboardComp->SetColor(GetLight()->LightColor);
	}
}

void ABullet::InitDefaultComponents()
{
    auto* Sphere = AddComponent<UStaticMeshComponent>();
    Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/sphere.obj"));
    Sphere->SetRelativeScale(FVector(0.5, 0.5, 0.5));
    SetRootComponent(Sphere);

	auto* BoxComp = AddComponent<UBoxComponent>();
    BoxComp->AttachToComponent(GetRootComponent());
    BoxComp->SetGenerateOverlapEvents(true);

    ProjectileComp = AddComponent<UProjectileMovementComponent>();
    ProjectileComp->SetInitialSpeed(10);
    ProjectileComp->SetComponentTickEnabled(true);
    ProjectileComp->SetUpdatedComponent(GetRootComponent());
}

void ABullet::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}

void ABullet::SetProjectileVelocity(FVector NewVelocity)
{
    if (ProjectileComp)
	    ProjectileComp->SetVelocity(NewVelocity);
}

void ADestructibleActor::InitDestructibleActor(UStaticMesh* InStaticMesh)
{
    ProcMeshComp = AddComponent<UProceduralMeshComponent>();
    ProcMeshComp->CreateFrom(InStaticMesh);
    SetRootComponent(ProcMeshComp);

    BoxComponent = AddComponent<UBoxComponent>();
    BoxComponent->AttachToComponent(GetRootComponent());
    BoxComponent->SetGenerateOverlapEvents(true);

    ProjMoveComp = AddComponent<UProjectileMovementComponent>();
    ProjMoveComp->SetVelocity(FVector(0, 0, 0));
    ProjMoveComp->SetInitialSpeed(0);
    ProjMoveComp->SetComponentTickEnabled(false);
    ProjMoveComp->SetGravityScale(0);
    ProjMoveComp->SetUpdatedComponent(GetRootComponent());
}

void ADestructibleActor::InitDestructibleActor(UProceduralMeshComponent* InProcMeshComp)
{
    ProcMeshComp = AddComponent<UProceduralMeshComponent>();
    ProcMeshComp->CreateFrom(InProcMeshComp);
    SetRootComponent(ProcMeshComp);

    BoxComponent = AddComponent<UBoxComponent>();
    BoxComponent->AttachToComponent(GetRootComponent());
	// 잘린 애들을 무한히 자를 수 없게 제한
    BoxComponent->SetGenerateOverlapEvents(true);

    ProjMoveComp = AddComponent<UProjectileMovementComponent>();
    ProjMoveComp->SetVelocity(FVector(0, 0, 0));
    ProjMoveComp->SetInitialSpeed(1);
    ProjMoveComp->SetComponentTickEnabled(true);
    ProjMoveComp->SetGravityScale(1);
    ProjMoveComp->SetUpdatedComponent(GetRootComponent());
}

void ADestructibleActor::InitDefaultComponents()
{
    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice.obj");
    // UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice_centered.obj");
    InitDestructibleActor(Mesh);
}

void ADestructibleActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}

void ADestructibleActor::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
}

void ADestructibleActor::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor)
        return;

    if (!OtherActor->IsA<ABladeSlash>())
    {
        AActor::OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
        return;
    }

    if (!OtherComp)
        return;

    const FTransform& BladeTransform = OtherComp->GetWorldTransform();
    FVector N_World;
    FVector P_World;

    N_World = BladeTransform.GetUnitAxis(EAxis::Y).GetSafeNormal();
    P_World = BladeTransform.GetLocation();

    // 메쉬 로컬 공간으로 변환
    FTransform MeshTransform = ProcMeshComp->GetWorldTransform();

	FQuat MeshRotation = MeshTransform.GetRotation();
    FMatrix RotationMatrix = MeshRotation.ToMatrix();    

    // 노멀은 회전만으로 역변환 (회전행렬은 직교행렬이라 전치 = 역행렬)
    FVector N_Local = RotationMatrix.GetTransposed().TransformVector(N_World).GetSafeNormal();

    // 위치는 풀 트랜스폼으로 역변환
    FVector P_Local = MeshTransform.InverseTransformPosition(P_World);

    FPlane SlicePlane;
    SlicePlane.Normal = N_Local;
    // SlicePlane.D = -FVector::DotProduct(N_Local, P_Local);
    SlicePlane.D = 0;

    // 절단된 메쉬를 담을 임시 컴포넌트 생성
    UProceduralMeshComponent* TempMesh1 = UObjectManager::Get().CreateObject<UProceduralMeshComponent>();
    UProceduralMeshComponent* TempMesh2 = UObjectManager::Get().CreateObject<UProceduralMeshComponent>();

    // 슬라이스 실행
    FMeshSlicer::SliceComponent(ProcMeshComp, SlicePlane, TempMesh1, TempMesh2);

    bool bFirstMeshEmpty = (TempMesh1->GetSections().empty() || TempMesh1->GetSections()[0].Vertices.empty());
    bool bSecondMeshEmpty = (TempMesh2->GetSections().empty() || TempMesh2->GetSections()[0].Vertices.empty());

    UWorld* World = GetFocusedWorld();
    float SeparateDistance = 1.0f;

    if (bFirstMeshEmpty && bSecondMeshEmpty)
    {
        // 둘 다 비어있으면 아무것도 안 함
    }
    else if (bFirstMeshEmpty || bSecondMeshEmpty)
    {
        // 굳이 새로 만들 필요 없음
    }
    else
    {
        ADestructibleActor* Actor1 = World->SpawnActor<ADestructibleActor>();
        ADestructibleActor* Actor2 = World->SpawnActor<ADestructibleActor>();
        Actor1->InitDestructibleActor(TempMesh1);
        Actor2->InitDestructibleActor(TempMesh2);
        Actor1->SetActorLocation(GetActorLocation());
        Actor1->SetActorRotation(GetActorRotation());
        Actor1->SetActorScale(GetActorScale());
        Actor2->SetActorLocation(GetActorLocation());
        Actor2->SetActorRotation(GetActorRotation());
        Actor2->SetActorScale(GetActorScale());
        Actor1->ProjMoveComp->SetVelocity(N_World * 5);
        Actor2->ProjMoveComp->SetVelocity(-N_World * 5);

        AActor::OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
        OtherActor->MarkPendingKill();
        this->MarkPendingKill();
    }
	
	UObjectManager::Get().DestroyObject(TempMesh1);
    UObjectManager::Get().DestroyObject(TempMesh2);
}

void ADestructibleActor::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}

void ADestructibleActor::PostDuplicate(UObject* Original)
{
	// 해당 함수에서 모든 걸 복사하기 때문에 추가로 복사를 고려할 필요 없음
    AActor::PostDuplicate(Original);

	// 복사된 것중 필요한 Comp 만 연결하는 과정
    ProcMeshComp = nullptr;
    BoxComponent = nullptr;
    ProjMoveComp = nullptr;

    for (UActorComponent* Comp : GetComponents())
    {
        if (!Comp) continue;
        if (ProcMeshComp == nullptr && Comp->IsA<UProceduralMeshComponent>())
        {
            ProcMeshComp = static_cast<UProceduralMeshComponent*>(Comp);
        }
        else if (BoxComponent == nullptr && Comp->IsA<UBoxComponent>())
        {
            BoxComponent = static_cast<UBoxComponent*>(Comp);
        }
		else if (ProjMoveComp == nullptr && Comp->IsA<UProjectileMovementComponent>())
		{
            ProjMoveComp = static_cast<UProjectileMovementComponent*>(Comp);
		}

        if (ProcMeshComp && BoxComponent && ProjMoveComp) break;
    }
}

void UMainSceneDestructibleComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

    bSlicePending = bAutoStart;
    bSliced = false;
    Elapsed = 0.0f;
    PatrolElapsed = 0.0f;
    Fragments.clear();
    FragmentStartLocations.clear();
    FragmentTargetLocations.clear();
}

bool UMainSceneDestructibleComponent::StartSlice()
{
    AMainSceneDestructibleActor* Destructible = Cast<AMainSceneDestructibleActor>(GetOwner());
    if (!Destructible)
    {
        return false;
    }

    TArray<AMainSceneDestructibleActor*> ActiveFragments;
    ActiveFragments.push_back(Destructible);

    const int32 RequestedSliceCount = std::max(1, SliceCount);
    int32 CompletedSliceCount = 0;
    for (int32 SliceIndex = 0; SliceIndex < RequestedSliceCount && !ActiveFragments.empty(); ++SliceIndex)
    {
        std::uniform_int_distribution<size_t> IndexDistribution(0, ActiveFragments.size() - 1);
        const size_t TargetIndex = IndexDistribution(GetMainSceneDestructibleRng());
        AMainSceneDestructibleActor* Target = ActiveFragments[TargetIndex];
        if (!Target || Target->IsPendingKill())
        {
            ActiveFragments.erase(ActiveFragments.begin() + TargetIndex);
            --SliceIndex;
            continue;
        }

        TArray<AMainSceneDestructibleActor*> NewFragments;
        for (int32 Attempt = 0; Attempt < 5 && NewFragments.empty(); ++Attempt)
        {
            NewFragments = Target->SliceForMainScene(
                Target->GetActorLocation(),
                RandomUnitVector(),
                0.0f);
        }

        if (NewFragments.empty())
        {
            continue;
        }

        Target->StopPresentationMotion();
        Target->SetVisible(false);
        ActiveFragments.erase(ActiveFragments.begin() + TargetIndex);
        for (AMainSceneDestructibleActor* Fragment : NewFragments)
        {
            if (Fragment)
            {
                ActiveFragments.push_back(Fragment);
            }
        }
        ++CompletedSliceCount;
    }

    Fragments = ActiveFragments;

    if (CompletedSliceCount == 0 || Fragments.empty())
    {
        UE_LOG_WARNING("[MainSceneDestructible] Slice failed: %s", Destructible->GetName().c_str());
        return false;
    }

    FragmentStartLocations.clear();
    FragmentTargetLocations.clear();
    const float DirectionDistance = SliceSpeed * SliceDuration;
    for (size_t Index = 0; Index < Fragments.size(); ++Index)
    {
        AMainSceneDestructibleActor* Fragment = Fragments[Index];
        if (!Fragment)
        {
            continue;
        }

        Fragment->StopPresentationMotion();
        const FVector StartLocation = Fragment->GetActorLocation();
        const FVector ScatterDirection = RandomUnitVector();
        const float DistanceScale = RandomRange(0.65f, 1.15f);
        FragmentStartLocations.push_back(StartLocation);
        FragmentTargetLocations.push_back(StartLocation + ScatterDirection * DirectionDistance * DistanceScale);
    }

    bSliced = true;
    Elapsed = 0.0f;
    PatrolElapsed = 0.0f;
    UE_LOG("[MainSceneDestructible] Slice started: %s Cuts=%d Fragments=%d", Destructible->GetName().c_str(), CompletedSliceCount, static_cast<int32>(Fragments.size()));
    return true;
}

void UMainSceneDestructibleComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << "Auto Start" << bAutoStart;
    Ar << "Slice Duration" << SliceDuration;
    Ar << "Slice Speed" << SliceSpeed;
    Ar << "Patrol Amplitude" << PatrolAmplitude;
    Ar << "Patrol Speed" << PatrolSpeed;
    Ar << "Slice Count" << SliceCount;

    if (Ar.IsLoading()
        && !bAutoStart
        && SliceDuration <= 0.0f
        && SliceSpeed <= 0.0f
        && PatrolAmplitude <= 0.0f
        && PatrolSpeed <= 0.0f)
    {
        bAutoStart = true;
        SliceDuration = 1.0f;
        SliceSpeed = 1.2f;
        PatrolAmplitude = 0.18f;
        PatrolSpeed = 1.15f;
    }

    if (Ar.IsLoading() && SliceCount <= 0)
    {
        SliceCount = 5;
    }
}

void UMainSceneDestructibleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    OutProps.push_back({"Auto Start", EPropertyType::Bool, &bAutoStart});
    OutProps.push_back({"Slice Duration", EPropertyType::Float, &SliceDuration, 0.05f, 10.0f, 0.05f});
    OutProps.push_back({"Slice Speed", EPropertyType::Float, &SliceSpeed, 0.0f, 10.0f, 0.05f});
    OutProps.push_back({"Patrol Amplitude", EPropertyType::Float, &PatrolAmplitude, 0.0f, 10.0f, 0.01f});
    OutProps.push_back({"Patrol Speed", EPropertyType::Float, &PatrolSpeed, 0.0f, 20.0f, 0.05f});
    OutProps.push_back({"Slice Count", EPropertyType::Int, &SliceCount, 1.0f, 12.0f, 1.0f});
}

float UMainSceneDestructibleComponent::GetRealDeltaTime(float DeltaTime) const
{
    return DeltaTime;
}

void UMainSceneDestructibleComponent::TickComponent(float DeltaTime)
{
    if (bSlicePending)
    {
        bSlicePending = false;
        StartSlice();
    }

    if (!bSliced)
    {
        return;
    }

    const float RealDeltaTime = GetRealDeltaTime(DeltaTime);
    Elapsed += RealDeltaTime;

    if (Elapsed < SliceDuration)
    {
        const float Alpha = SliceDuration > 0.0f ? Elapsed / SliceDuration : 1.0f;
        const float EaseOut = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);

        for (size_t Index = 0; Index < Fragments.size() && Index < FragmentStartLocations.size() && Index < FragmentTargetLocations.size(); ++Index)
        {
            AMainSceneDestructibleActor* Fragment = Fragments[Index];
            if (!Fragment || Fragment->IsPendingKill())
            {
                continue;
            }

            const FVector Location = FragmentStartLocations[Index] + (FragmentTargetLocations[Index] - FragmentStartLocations[Index]) * EaseOut;
            Fragment->SetActorLocation(Location);
        }
        return;
    }

    PatrolElapsed += RealDeltaTime;
    const float Offset = std::sin(PatrolElapsed * PatrolSpeed) * PatrolAmplitude;

    for (size_t Index = 0; Index < Fragments.size() && Index < FragmentTargetLocations.size(); ++Index)
    {
        AMainSceneDestructibleActor* Fragment = Fragments[Index];
        if (!Fragment || Fragment->IsPendingKill())
        {
            continue;
        }

        const float Direction = (Index % 2 == 0) ? 1.0f : -1.0f;
        Fragment->SetActorLocation(FragmentTargetLocations[Index] + FVector::UpVector * Offset * Direction);
    }
}

void AMainSceneDestructibleActor::InitDefaultComponents()
{
    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice.obj");
    InitFromStaticMesh(Mesh, true);
}

void AMainSceneDestructibleActor::InitFromStaticMesh(UStaticMesh* StaticMesh, bool bAddPresentationComponent)
{
    ProcMeshComp = AddComponent<UProceduralMeshComponent>();
    ProcMeshComp->CreateFrom(StaticMesh);
    SetRootComponent(ProcMeshComp);

    BoxComponent = AddComponent<UBoxComponent>();
    BoxComponent->AttachToComponent(GetRootComponent());
    BoxComponent->SetGenerateOverlapEvents(false);

    ProjMoveComp = AddComponent<UProjectileMovementComponent>();
    ProjMoveComp->SetVelocity(FVector(0, 0, 0));
    ProjMoveComp->SetInitialSpeed(0);
    ProjMoveComp->SetComponentTickEnabled(false);
    ProjMoveComp->SetGravityScale(0);
    ProjMoveComp->SetUpdatedComponent(GetRootComponent());

    if (bAddPresentationComponent)
    {
        PresentationComponent = AddComponent<UMainSceneDestructibleComponent>();
    }
}

void AMainSceneDestructibleActor::InitFromProceduralMesh(UProceduralMeshComponent* InProcMeshComp, bool bAddPresentationComponent)
{
    ProcMeshComp = AddComponent<UProceduralMeshComponent>();
    ProcMeshComp->CreateFrom(InProcMeshComp);
    SetRootComponent(ProcMeshComp);

    BoxComponent = AddComponent<UBoxComponent>();
    BoxComponent->AttachToComponent(GetRootComponent());
    BoxComponent->SetGenerateOverlapEvents(false);

    ProjMoveComp = AddComponent<UProjectileMovementComponent>();
    ProjMoveComp->SetVelocity(FVector(0, 0, 0));
    ProjMoveComp->SetInitialSpeed(0);
    ProjMoveComp->SetComponentTickEnabled(true);
    ProjMoveComp->SetGravityScale(0);
    ProjMoveComp->SetUpdatedComponent(GetRootComponent());

    if (bAddPresentationComponent)
    {
        PresentationComponent = AddComponent<UMainSceneDestructibleComponent>();
    }
}

void AMainSceneDestructibleActor::PostComponentRegistered(UActorComponent* Comp)
{
    AActor::PostComponentRegistered(Comp);

    if (!Comp)
    {
        return;
    }

    if (!ProcMeshComp && Comp->IsA<UProceduralMeshComponent>())
    {
        ProcMeshComp = static_cast<UProceduralMeshComponent*>(Comp);
    }
    else if (!BoxComponent && Comp->IsA<UBoxComponent>())
    {
        BoxComponent = static_cast<UBoxComponent*>(Comp);
    }
    else if (!ProjMoveComp && Comp->IsA<UProjectileMovementComponent>())
    {
        ProjMoveComp = static_cast<UProjectileMovementComponent*>(Comp);
    }
    else if (!PresentationComponent && Comp->IsA<UMainSceneDestructibleComponent>())
    {
        PresentationComponent = static_cast<UMainSceneDestructibleComponent*>(Comp);
    }
}

void AMainSceneDestructibleActor::PostDuplicate(UObject* Original)
{
    AActor::PostDuplicate(Original);
    RebindComponents();
}

void AMainSceneDestructibleActor::RebindComponents()
{
    ProcMeshComp = nullptr;
    BoxComponent = nullptr;
    ProjMoveComp = nullptr;
    PresentationComponent = nullptr;

    for (UActorComponent* Comp : GetComponents())
    {
        if (!Comp)
        {
            continue;
        }

        if (!ProcMeshComp && Comp->IsA<UProceduralMeshComponent>())
        {
            ProcMeshComp = static_cast<UProceduralMeshComponent*>(Comp);
        }
        else if (!BoxComponent && Comp->IsA<UBoxComponent>())
        {
            BoxComponent = static_cast<UBoxComponent*>(Comp);
        }
        else if (!ProjMoveComp && Comp->IsA<UProjectileMovementComponent>())
        {
            ProjMoveComp = static_cast<UProjectileMovementComponent*>(Comp);
        }
        else if (!PresentationComponent && Comp->IsA<UMainSceneDestructibleComponent>())
        {
            PresentationComponent = static_cast<UMainSceneDestructibleComponent*>(Comp);
        }
    }
}

TArray<AMainSceneDestructibleActor*> AMainSceneDestructibleActor::SliceForMainScene(
    const FVector& PlanePointWorld,
    const FVector& PlaneNormalWorld,
    float SeparateSpeed)
{
    (void)PlanePointWorld;

    TArray<AMainSceneDestructibleActor*> Result;
    if (!ProcMeshComp)
    {
        RebindComponents();
    }
    if (!ProcMeshComp)
    {
        return Result;
    }

    FVector PlanePointLocal;
    if (!GetProceduralMeshLocalCenter(ProcMeshComp, PlanePointLocal))
    {
        UStaticMesh* FallbackMesh = FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice.obj");
        if (FallbackMesh)
        {
            ProcMeshComp->CreateFrom(FallbackMesh);
        }
    }

    if (!GetProceduralMeshLocalCenter(ProcMeshComp, PlanePointLocal))
    {
        UE_LOG_WARNING("[MainSceneDestructible] Slice failed: empty procedural mesh");
        return Result;
    }

    const FVector SafeNormalWorld = PlaneNormalWorld.GetSafeNormal();
    if (SafeNormalWorld.SizeSquared() <= 0.000001f)
    {
        return Result;
    }

    const FTransform MeshTransform = ProcMeshComp->GetWorldTransform();
    const FQuat MeshRotation = MeshTransform.GetRotation();
    const FMatrix RotationMatrix = MeshRotation.ToMatrix();

    const FVector PlaneNormalLocal = RotationMatrix.GetTransposed().TransformVector(SafeNormalWorld).GetSafeNormal();

    FPlane SlicePlane;
    SlicePlane.Normal = PlaneNormalLocal;
    SlicePlane.D = -FVector::DotProduct(PlaneNormalLocal, PlanePointLocal);

    UProceduralMeshComponent* TempMesh1 = UObjectManager::Get().CreateObject<UProceduralMeshComponent>();
    UProceduralMeshComponent* TempMesh2 = UObjectManager::Get().CreateObject<UProceduralMeshComponent>();

    FMeshSlicer::SliceComponent(ProcMeshComp, SlicePlane, TempMesh1, TempMesh2);

    const bool bFirstMeshEmpty = (TempMesh1->GetSections().empty() || TempMesh1->GetSections()[0].Vertices.empty());
    const bool bSecondMeshEmpty = (TempMesh2->GetSections().empty() || TempMesh2->GetSections()[0].Vertices.empty());

    UWorld* World = GetFocusedWorld();
    if (!bFirstMeshEmpty && !bSecondMeshEmpty && World)
    {
        AMainSceneDestructibleActor* Actor1 = World->SpawnActor<AMainSceneDestructibleActor>();
        AMainSceneDestructibleActor* Actor2 = World->SpawnActor<AMainSceneDestructibleActor>();
        Actor1->InitFromProceduralMesh(TempMesh1, false);
        Actor2->InitFromProceduralMesh(TempMesh2, false);

        Actor1->SetActorLocation(GetActorLocation());
        Actor1->SetActorRotation(GetActorRotation());
        Actor1->SetActorScale(GetActorScale());
        Actor2->SetActorLocation(GetActorLocation());
        Actor2->SetActorRotation(GetActorRotation());
        Actor2->SetActorScale(GetActorScale());

        Actor1->ProjMoveComp->SetVelocity(SafeNormalWorld * SeparateSpeed);
        Actor2->ProjMoveComp->SetVelocity(-SafeNormalWorld * SeparateSpeed);

        Result.push_back(Actor1);
        Result.push_back(Actor2);
    }
    UObjectManager::Get().DestroyObject(TempMesh1);
    UObjectManager::Get().DestroyObject(TempMesh2);
    return Result;
}

void AMainSceneDestructibleActor::StopPresentationMotion()
{
    if (!ProjMoveComp)
    {
        RebindComponents();
    }

    if (ProjMoveComp)
    {
        ProjMoveComp->SetVelocity(FVector::ZeroVector);
        ProjMoveComp->SetGravityScale(0.0f);
        ProjMoveComp->SetComponentTickEnabled(false);
    }
}

void ABladeSlash::InitDefaultComponents()
{
    auto* Root = AddComponent<UBoxComponent>();
    Root->SetGenerateOverlapEvents(true);
    SetRootComponent(Root);

	// Slash 경로 시각화 용도
    //auto* Cube = AddComponent<UStaticMeshComponent>();
    //Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
    //Cube->AttachToComponent(Root);
}

void ABladeSlash::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}
