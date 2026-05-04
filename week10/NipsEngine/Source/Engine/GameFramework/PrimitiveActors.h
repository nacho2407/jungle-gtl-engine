#pragma once

#include "AActor.h"
#include "Core/Delegates/Delegate.h"
#include "Core/CollisionTypes.h"

class UTextRenderComponent;
class UDecalComponent;
class ULightComponent;
class UBillboardComponent;
class UHeightFogComponent;
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UProjectileMovementComponent;
class UProceduralMeshComponent;
class UStaticMesh;

class ACubeActor : public AActor
{
public:
	DECLARE_CLASS(ACubeActor, AActor)
	ACubeActor() = default;

	void InitDefaultComponents();
};

class ASphereActor : public AActor
{
public:
	DECLARE_CLASS(ASphereActor, AActor)
	ASphereActor() = default;

	void InitDefaultComponents();
};

class APlaneActor : public AActor
{
public:
	DECLARE_CLASS(APlaneActor, AActor)
	APlaneActor() = default;

	void InitDefaultComponents();
};

class AAttachTestActor : public AActor
{
public:
	DECLARE_CLASS(AAttachTestActor, AActor)
	AAttachTestActor() = default;

	void InitDefaultComponents();
};

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

//  PIE 용 임시 액터
class ADefaultPlayerActor : public ASceneActor
{
public:
	DECLARE_CLASS(ADefaultPlayerActor, AActor)
	ADefaultPlayerActor() = default;

	void InitDefaultComponents() override;
	UCameraComponent* GetCameraComponent() const { return CameraComp; }
	USpringArmComponent* GetSpringArmComponent() const { return SpringArmComp; }
    
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
    void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	void OnTakeDamage(int32 InDamage) { Health = std::max(0, Health - InDamage); }

private:
	USpringArmComponent* SpringArmComp = nullptr;
	UCameraComponent* CameraComp = nullptr;

	int32 Health = 100;
};

class APlayerStart : public AActor
{
public:
	DECLARE_CLASS(APlayerStart, AActor)
	APlayerStart() = default;

	void InitDefaultComponents() override;
};

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

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents();
};

class ASubUVActor : public AActor
{
public:
    DECLARE_CLASS(ASubUVActor, AActor)
    ASubUVActor() = default;

    void InitDefaultComponents();
};

class ATextRenderActor : public AActor
{
public:
    DECLARE_CLASS(ATextRenderActor, AActor)
    ATextRenderActor() = default;

    void InitDefaultComponents();
};

class ABillboardActor : public AActor
{
public:
    DECLARE_CLASS(ABillboardActor, AActor)
	ABillboardActor() = default;

    void InitDefaultComponents();
};

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)
	ADecalActor() = default;

	void InitDefaultComponents();
};

class AFireballActor : public AActor {
public:
    DECLARE_CLASS(AFireballActor, AActor)
	AFireballActor() = default;
	
	void InitDefaultComponents();
};

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

class AAmbientLightActor : public ALightActor
{
public:
    DECLARE_CLASS(AAmbientLightActor, ALightActor)
    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
};

class ADirectionalLightActor : public ALightActor
{
public:
    DECLARE_CLASS(ADirectionalLightActor, ALightActor)
    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
};

class APointLightActor : public ALightActor
{
public:
    DECLARE_CLASS(APointLightActor, ALightActor)
    virtual void InitDefaultComponents() override;
    virtual void Tick(float DeltaTime) override;
};

class ASpotlightActor : public APointLightActor 
{
public:
	DECLARE_CLASS(ASpotlightActor, APointLightActor)
	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

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

class ABladeSlash : public AActor
{
public:
    DECLARE_CLASS(ABladeSlash, AActor)
    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
};

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

class AMainSceneDestructibleActor;

class UMainSceneDestructibleComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UMainSceneDestructibleComponent, UActorComponent)

    void BeginPlay() override;
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

protected:
    void TickComponent(float DeltaTime) override;

private:
    float GetRealDeltaTime(float DeltaTime) const;
    bool StartSlice();

    bool bAutoStart = true;
    float SliceDuration = 1.0f;
    float SliceSpeed = 1.2f;
    float PatrolAmplitude = 0.18f;
    float PatrolSpeed = 1.15f;
    int32 SliceCount = 5;

    bool bSlicePending = false;
    bool bSliced = false;
    float Elapsed = 0.0f;
    float PatrolElapsed = 0.0f;
    TArray<AMainSceneDestructibleActor*> Fragments;
    TArray<FVector> FragmentStartLocations;
    TArray<FVector> FragmentTargetLocations;
};

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
