#pragma once

#include "AActor.h"
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

UCLASS(Placeable, DisplayName = "Fog", Category = "Basic")
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

UCLASS(Placeable, DisplayName = "Decal", Category = "Basic")
class ADecalActor : public AActor
{
public:
	GENERATED_BODY(ADecalActor, AActor)
	ADecalActor() = default;

	void InitDefaultComponents();
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
