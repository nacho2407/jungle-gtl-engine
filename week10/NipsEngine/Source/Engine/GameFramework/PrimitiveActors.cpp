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


namespace
{
	constexpr const char* CubeMeshPath = "Asset/Mesh/Cube.obj";
	constexpr const char* FireballMeshPath = "Asset/Mesh/Sun/sun.obj";
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

DEFINE_CLASS(ABladeSlash, AActor)
REGISTER_FACTORY(ABladeSlash)

void ACubeActor::InitDefaultComponents()
{
	auto* Cube = AddComponent<UStaticMeshComponent>();
    // Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Lumine/LumineModel.obj"));
    // Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
    Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice.obj"));
    SetRootComponent(Cube);

	// Text
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetFont(FName("Default"));
	Text->AttachToComponent(Cube);
	Text->SetText("UUID: " + std::to_string(GetUUID()));
	Text->SetTransient(true);
	Text->SetEditorOnly(true);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));

	UProceduralMeshComponent* ProcMeshComp1 = AddComponent<UProceduralMeshComponent>();
    UProceduralMeshComponent* ProcMeshComp2 = AddComponent<UProceduralMeshComponent>();
    
	FPlane Plane;
    Plane.Normal = FVector(0, 1, 1);
    Plane.D = 0;
    FMeshSlicer::SliceComponent(Cube, Plane, ProcMeshComp1, ProcMeshComp2);
	
	ProcMeshComp1->AttachToComponent(Cube);
    ProcMeshComp2->AttachToComponent(Cube);
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

	UStaticMeshComponent* DebugBody = AddComponent<UStaticMeshComponent>();
	DebugBody->AttachToComponent(SceneRoot);
	DebugBody->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
	DebugBody->SetRelativeLocation(FVector(0.0f, 0.0f, 0.5f));
	DebugBody->SetRelativeScale(FVector(0.4f, 0.4f, 1.0f));
	DebugBody->SetEnableCull(false);

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
}

void ADestructibleActor::InitDestructibleActor(UProceduralMeshComponent* InProcMeshComp)
{
    ProcMeshComp = AddComponent<UProceduralMeshComponent>();
    ProcMeshComp->CreateFrom(InProcMeshComp);
    UObjectManager::Get().DestroyObject(InProcMeshComp);
    SetRootComponent(ProcMeshComp);

    BoxComponent = AddComponent<UBoxComponent>();
    BoxComponent->AttachToComponent(GetRootComponent());
	// 잘린 애들을 무한히 자를 수 없게 제한
    BoxComponent->SetGenerateOverlapEvents(false);
}

void ADestructibleActor::InitDefaultComponents()
{
    UStaticMesh* Mesh = FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice.obj");
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
	if (OtherActor->IsA<ABladeSlash>())
	{
        UProceduralMeshComponent* ProcMeshComp1 = UObjectManager::Get().CreateObject<UProceduralMeshComponent>();
        UProceduralMeshComponent* ProcMeshComp2 = UObjectManager::Get().CreateObject<UProceduralMeshComponent>();

        FPlane Plane;

        FVector N = SweepResult.Normal;   // 반드시 normalize
        FVector P = SweepResult.Location; // plane 위의 점
        float D = FVector::DotProduct(N, P);

        FVector SplitDir;
        SplitDir = FVector::CrossProduct(N, FVector::ForwardVector);
        if (SplitDir.IsNearlyZero())
        {
            SplitDir = FVector::CrossProduct(N, FVector::RightVector);
        }

        Plane.Normal = SplitDir;
        Plane.D = 0;

        FMeshSlicer::SliceComponent(ProcMeshComp, Plane, ProcMeshComp1, ProcMeshComp2);

        UWorld* World = OtherActor->GetFocusedWorld();
        ADestructibleActor* Actor1 = World->SpawnActor<ADestructibleActor>();
        ADestructibleActor* Actor2 = World->SpawnActor<ADestructibleActor>();

        Actor1->InitDestructibleActor(ProcMeshComp1);
        Actor1->SetActorLocation(GetActorLocation() + FVector(0, 0, -1) * GetActorScale().Size() / 2);
        Actor1->SetActorScale(GetActorScale());
        Actor1->SetActorRotation(GetActorRotation());

        Actor2->InitDestructibleActor(ProcMeshComp2);
        Actor2->SetActorLocation(GetActorLocation() + FVector(0, 0, 1) * GetActorScale().Size() / 2);
        Actor2->SetActorScale(GetActorScale());
        Actor2->SetActorRotation(GetActorRotation());
	}
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
        if (ProcMeshComp && BoxComponent) break;
    }
}

void ABladeSlash::InitDefaultComponents()
{
    auto* Root = AddComponent<UBoxComponent>();
    Root->SetGenerateOverlapEvents(true);
    SetRootComponent(Root);
}

void ABladeSlash::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}
