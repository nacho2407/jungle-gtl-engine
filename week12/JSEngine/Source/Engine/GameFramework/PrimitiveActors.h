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

UCLASS()
class ACubeActor : public AActor
{
public:
	DECLARE_CLASS(ACubeActor, AActor)
	ACubeActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ASphereActor : public AActor
{
public:
	DECLARE_CLASS(ASphereActor, AActor)
	ASphereActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class APlaneActor : public AActor
{
public:
	DECLARE_CLASS(APlaneActor, AActor)
	APlaneActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class AAttachTestActor : public AActor
{
public:
	DECLARE_CLASS(AAttachTestActor, AActor)
	AAttachTestActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ASceneActor : public AActor
{
public:
	DECLARE_CLASS(ASceneActor, AActor)
	ASceneActor() = default;

	void InitDefaultComponents();

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
};

UCLASS()
class APlayerStart : public AActor
{
public:
	DECLARE_CLASS(APlayerStart, AActor)
	APlayerStart() = default;

	void InitDefaultComponents() override;
};

UCLASS()
class AFogActor : public AActor
{
public:
	DECLARE_CLASS(AFogActor, AActor)
	AFogActor() = default;

	void InitDefaultComponents();

	UHeightFogComponent* GetFogComponent() const { return FogComp; }

private:
	UHeightFogComponent* FogComp = nullptr;
	UBillboardComponent* BillboardComp = nullptr;
};

UCLASS()
class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ASkeletalMeshActor : public AActor
{
public:
	DECLARE_CLASS(ASkeletalMeshActor, AActor)
	ASkeletalMeshActor() = default;

	void InitDefaultComponents();

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComp; }

private:
	USkeletalMeshComponent* SkeletalMeshComp = nullptr;
};

UCLASS()
class ASubUVActor : public AActor
{
public:
	DECLARE_CLASS(ASubUVActor, AActor)
	ASubUVActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ATextRenderActor : public AActor
{
public:
	DECLARE_CLASS(ATextRenderActor, AActor)
	ATextRenderActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ABillboardActor : public AActor
{
public:
	DECLARE_CLASS(ABillboardActor, AActor)
	ABillboardActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)
	ADecalActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class AFireballActor : public AActor {
public:
	DECLARE_CLASS(AFireballActor, AActor)
	AFireballActor() = default;
	
	void InitDefaultComponents();
};

UCLASS()
class ADecalSpotLightActor : public AActor {
public:
	DECLARE_CLASS(ADecalSpotLightActor, AActor)
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
	DECLARE_CLASS(ALightActor, AActor)
	ALightActor() = default;

	ULightComponent* GetLight() const;
	void SetLight(ULightComponent* InLight);

	UBillboardComponent* GetBillboard() const { return BillboardComp; }
	void SetBillboard(UBillboardComponent* InBillboard) { BillboardComp = InBillboard; }

protected:
	ULightComponent* LightComp = nullptr;
	UBillboardComponent* BillboardComp = nullptr;
};

UCLASS()
class AAmbientLightActor : public ALightActor
{
public:
	DECLARE_CLASS(AAmbientLightActor, ALightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS()
class ADirectionalLightActor : public ALightActor
{
public:
	DECLARE_CLASS(ADirectionalLightActor, ALightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS()
class APointLightActor : public ALightActor
{
public:
	DECLARE_CLASS(APointLightActor, ALightActor)
	virtual void InitDefaultComponents() override;
	virtual void Tick(float DeltaTime) override;
};

UCLASS()
class ASpotlightActor : public APointLightActor 
{
public:
	DECLARE_CLASS(ASpotlightActor, APointLightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS()
class ABullet : public AActor
{
public:
	DECLARE_CLASS(ABullet, AActor)
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
	DECLARE_CLASS(ABladeSlash, AActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS()
class ADestructibleActor : public AActor
{
public:
	DECLARE_CLASS(ADestructibleActor, AActor)

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

UCLASS()
class ABoundsBoxActor : public AActor
{
public:
	DECLARE_CLASS(ABoundsBoxActor, AActor)
	void InitDefaultComponents() override;

	void Tick(float DeltaTime) override;

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	void PostDuplicate(UObject* Original) override;
private:
	UBoxComponent* BoxComponent = nullptr;
};

UCLASS()
class AMainSceneDestructibleActor : public AActor
{
public:
	DECLARE_CLASS(AMainSceneDestructibleActor, AActor)

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
