#pragma once

#include "AActor.h"
#include "Component/MainSceneDestructibleComponent.h"
#include "Core/CollisionTypes.h"

class UTextRenderComponent;
class UDecalComponent;
class ULightComponent;
class UBillboardComponent;
class UHeightFogComponent;
class UBoxComponent;
class UProjectileMovementComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class USkeletalMeshComponent;

UCLASS(Placeable, DisplayName = "Cube", Category = "Geometry")
class ACubeActor : public AActor
{
public:
	GENERATED_BODY(ACubeActor, AActor)
	ACubeActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Sphere", Category = "Geometry")
class ASphereActor : public AActor
{
public:
	GENERATED_BODY(ASphereActor, AActor)
	ASphereActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Plane", Category = "Geometry")
class APlaneActor : public AActor
{
public:
	GENERATED_BODY(APlaneActor, AActor)
	APlaneActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class AAttachTestActor : public AActor
{
public:
	GENERATED_BODY(AAttachTestActor, AActor)
	AAttachTestActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Empty Actor", Category = "Basic")
class ASceneActor : public AActor
{
public:
	GENERATED_BODY(ASceneActor, AActor)
	ASceneActor() = default;

	void InitDefaultComponents();

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
};

UCLASS(Placeable, DisplayName = "Player Start", Category = "Gameplay")
class APlayerStart : public AActor
{
public:
	GENERATED_BODY(APlayerStart, AActor)
	APlayerStart() = default;

	void InitDefaultComponents() override;
};

UCLASS(Placeable, DisplayName = "Fog", Category = "Environment")
class AFogActor : public AActor
{
public:
	GENERATED_BODY(AFogActor, AActor)
	AFogActor() = default;

	void InitDefaultComponents();

	UHeightFogComponent* GetFogComponent() const { return FogComp; }

private:
	UHeightFogComponent* FogComp = nullptr;
	UBillboardComponent* BillboardComp = nullptr;
};

UCLASS(Placeable, DisplayName = "Static Mesh", Category = "Basic")
class AStaticMeshActor : public AActor
{
public:
	GENERATED_BODY(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Skeletal Mesh", Category = "Basic")
class ASkeletalMeshActor : public AActor
{
public:
	GENERATED_BODY(ASkeletalMeshActor, AActor)
	ASkeletalMeshActor() = default;

	void InitDefaultComponents();

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComp; }

private:
	USkeletalMeshComponent* SkeletalMeshComp = nullptr;
};

UCLASS(Placeable, DisplayName = "SubUV", Category = "Basic")
class ASubUVActor : public AActor
{
public:
	GENERATED_BODY(ASubUVActor, AActor)
	ASubUVActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Text Render", Category = "Basic")
class ATextRenderActor : public AActor
{
public:
	GENERATED_BODY(ATextRenderActor, AActor)
	ATextRenderActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Billboard", Category = "Basic")
class ABillboardActor : public AActor
{
public:
	GENERATED_BODY(ABillboardActor, AActor)
	ABillboardActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Decal", Category = "Environment")
class ADecalActor : public AActor
{
public:
	GENERATED_BODY(ADecalActor, AActor)
	ADecalActor() = default;

	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Fireball", Category = "Gameplay")
class AFireballActor : public AActor {
public:
	GENERATED_BODY(AFireballActor, AActor)
	AFireballActor() = default;
	
	void InitDefaultComponents();
};

UCLASS(Placeable, DisplayName = "Decal Spotlight", Category = "Light")
class ADecalSpotLightActor : public AActor {
public:
	GENERATED_BODY(ADecalSpotLightActor, AActor)
	ADecalSpotLightActor() = default;

	void InitDefaultComponents();

	void Tick(float DeltaTime) override;

	const float GetRange() const { return Range; }
	void SetRange(float InRange) { Range = InRange; }

	const float GetAngle() const { return Angle; }
	void SetAngle(float InAngle) { Angle = InAngle; }

private:
	UDecalComponent* DecalComp = nullptr;

	float Range = 10.0f;
	float Angle = 30.0f;
};

UCLASS()
class ALightActor : public AActor
{
public:
	GENERATED_BODY(ALightActor, AActor)
	ALightActor() = default;

	ULightComponent* GetLight() const;
	void SetLight(ULightComponent* InLight);

	UBillboardComponent* GetBillboard() const { return BillboardComp; }
	void SetBillboard(UBillboardComponent* InBillboard) { BillboardComp = InBillboard; }

protected:
	ULightComponent* LightComp = nullptr;
	UBillboardComponent* BillboardComp = nullptr;
};

UCLASS(Placeable, DisplayName = "Ambient Light", Category = "Light")
class AAmbientLightActor : public ALightActor
{
public:
	GENERATED_BODY(AAmbientLightActor, ALightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS(Placeable, DisplayName = "Directional Light", Category = "Light")
class ADirectionalLightActor : public ALightActor
{
public:
	GENERATED_BODY(ADirectionalLightActor, ALightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS(Placeable, DisplayName = "Point Light", Category = "Light")
class APointLightActor : public ALightActor
{
public:
	GENERATED_BODY(APointLightActor, ALightActor)
	virtual void InitDefaultComponents() override;
	virtual void Tick(float DeltaTime) override;
};

UCLASS(Placeable, DisplayName = "Spot Light", Category = "Light")
class ASpotlightActor : public APointLightActor 
{
public:
	GENERATED_BODY(ASpotlightActor, APointLightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS()
class ABullet : public AActor
{
public:
	GENERATED_BODY(ABullet, AActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;

	void SetProjectileVelocity(FVector NewVelocity);

private:
	UProjectileMovementComponent* ProjectileComp = nullptr;
};

UCLASS()
class ABladeSlash : public AActor
{
public:
	GENERATED_BODY(ABladeSlash, AActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS(Placeable, DisplayName = "Destructible", Category = "Gameplay")
class ADestructibleActor : public AActor
{
public:
	GENERATED_BODY(ADestructibleActor, AActor)

	// 데이터를 입력으로 받아 초기화
	void InitDestructibleActor(UStaticMesh* StaticMesh);
	void InitDestructibleActor(UProceduralMeshComponent* InProcMeshComp);

	// 따로 StaticMesh 지정 안할 시 임의의 StaticMesh 로 초기화
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	void PostDuplicate(UObject* Original) override;

	uint32 GetSliceCount() const { return SliceCount; }
	void SetSliceCount(uint32 NewSliceCount) { SliceCount = NewSliceCount; }

private:
	UProceduralMeshComponent* ProcMeshComp = nullptr;
	UBoxComponent* BoxComponent = nullptr;
	// 물리 시뮬레이션 흉내용
	UProjectileMovementComponent* ProjMoveComp = nullptr;
	// 현재까지 잘려진 횟수
	uint32 SliceCount = 0;
};

UCLASS(Placeable, DisplayName = "Bounding Box", Category = "Debug")
class ABoundsBoxActor : public AActor
{
public:
	GENERATED_BODY(ABoundsBoxActor, AActor)
	void InitDefaultComponents() override;

	void Tick(float DeltaTime) override;

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	void PostDuplicate(UObject* Original) override;
private:
	UBoxComponent* BoxComponent = nullptr;
};

UCLASS(Placeable, DisplayName = "Main Scene Destructible", Category = "Gameplay")
class AMainSceneDestructibleActor : public AActor
{
public:
	GENERATED_BODY(AMainSceneDestructibleActor, AActor)

	void InitDefaultComponents() override;
	void PostComponentRegistered(UActorComponent* Comp) override;
	void PostDuplicate(UObject* Original) override;

	TArray<AMainSceneDestructibleActor*> SliceForMainScene(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld, float SeparateSpeed);
	void StopPresentationMotion();

private:
	void InitFromStaticMesh(UStaticMesh* StaticMesh, bool bAddPresentationComponent);
	void InitFromProceduralMesh(UProceduralMeshComponent* InProcMeshComp, bool bAddPresentationComponent);
	void RebindComponents();

	UProceduralMeshComponent* ProcMeshComp = nullptr;
	UBoxComponent* BoxComponent = nullptr;
	UProjectileMovementComponent* ProjMoveComp = nullptr;
	UMainSceneDestructibleComponent* PresentationComponent = nullptr;
};
