#include "GameFramework/PrimitiveActors.h"

#include "GameFramework/Pawn.h"
#include "Component/FireballComponent.h"
#include "Component/DecalComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/HeightFogComponent.h"

#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"

#include "Component/Movement/RotatingMovementComponent.h"
#include "Core/EditorResourcePaths.h"
#include "Core/ResourceManager.h"
#include <format>
#include <Component/SubUVComponent.h>
#include "Core/Debug.h"
#include "Component/BoxComponent.h"
#include "Core/CollisionTypes.h"
#include "Component/ProceduralMeshComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "GameFramework/World.h"
#include "Runtime/Script/ScriptManager.h"

#include <algorithm>
#include <cfloat>

#include "Animation/AnimSequence.h"

void ASceneActor::InitDefaultComponents()
{
	auto SceneRoot = AddComponent<USceneComponent>();
	SetRootComponent(SceneRoot);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(SceneRoot);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName(FEditorResourcePaths::Icon("EmptyActor.png"));
}

void APlayerStart::InitDefaultComponents()
{
	auto* SceneRoot = AddComponent<USceneComponent>();
	SetRootComponent(SceneRoot);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(SceneRoot);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName(FEditorResourcePaths::Icon("PlayerStart_64x.PNG"));
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
	Billboard->SetTextureName(FEditorResourcePaths::Icon("ExpoHeightFog_64x.png"));
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
}

void ASkeletalMeshActor::InitDefaultComponents()
{
	SkeletalMeshComp = AddComponent<USkeletalMeshComponent>();
	SkeletalMeshComp->SetSkeletalMesh(FResourceManager::Get().LoadSkeletalMesh("Asset/SkeletalMesh/Ahri/Ahri.fbx"));
	SetRootComponent(SkeletalMeshComp);

	// ─────────────────────────────────────────────────────────────────────
	// Socket Save/Load 검증용 테스트 ? 에디터에서 "HandSocket"을 정의·저장 후
	// 재시작하면 이 코드가 cube를 그 socket에 attach해서 위치를 시각화함.
	// transient + editorOnly로 scene 저장/게임 빌드 양쪽에서 자동으로 제외.
	// ─────────────────────────────────────────────────────────────────────
	//{
	//    const FName HandSocketName("HandSocket");
	//    USkeletalMesh* Mesh = SkeletalMeshComp->GetSkeletalMesh();
	//    if (Mesh && Mesh->HasSocket(HandSocketName))
	//    {
	//        UStaticMeshComponent* HandTest = AddComponent<UStaticMeshComponent>();
	//        HandTest->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
	//        HandTest->SetTransient(true);
	//        HandTest->SetEditorOnly(true);
	//        HandTest->AttachToComponent(SkeletalMeshComp, HandSocketName);
	//        UE_LOG("[SocketTest] HandSocket found on %s ? attached test cube",
	//               Mesh->GetAssetPathFileName().c_str());
	//    }
	//    else if (Mesh)
	//    {
	//        UE_LOG("[SocketTest] HandSocket not found on %s ? create via editor and Save",
	//               Mesh->GetAssetPathFileName().c_str());
	//    }
	//}
}

void ASubUVActor::InitDefaultComponents()
{
	SetTickInEditor(true); // Editor Tick을 받도록 변경

	auto* SubUV = AddComponent<USubUVComponent>();
	SetRootComponent(SubUV);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);
}

void ATextRenderActor::InitDefaultComponents()
{
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	SetRootComponent(Text);
	Text->SetFont(FName("Default"));
	Text->SetText("TextRender");
}

void ABillboardActor::InitDefaultComponents()
{
	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	SetRootComponent(Billboard);
	Billboard->SetTextureName(FEditorResourcePaths::Icon("Pawn_64x.png"));
}

void ADecalActor::InitDefaultComponents()
{
	UDecalComponent* Decal = AddComponent<UDecalComponent>();
	SetRootComponent(Decal);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->AttachToComponent(Decal);
	Billboard->SetEditorOnly(true);
	Billboard->SetTextureName(FEditorResourcePaths::Icon("DecalActor_64.png"));
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
	Billboard->SetTextureName(FEditorResourcePaths::Icon("SkyLight_64x.png"));
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
	Billboard->SetTextureName(FEditorResourcePaths::Icon("DirectionalLight_64x.png"));
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
	Billboard->SetTextureName(FEditorResourcePaths::Icon("PointLight_64x.png"));
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

void ASpotlightActor::InitDefaultComponents()
{
	SetTickInEditor(true);

	USpotlightComponent* Spot = AddComponent<USpotlightComponent>();
	SetRootComponent(Spot);

	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	Billboard->SetTextureName(FEditorResourcePaths::Icon("SpotLight_64x.png"));
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
