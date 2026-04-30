#pragma once

#include "AActor.h"
#include "Core/Delegates/Delegate.h"

class UTextRenderComponent;
class UDecalComponent;
class ULightComponent;
class UBillboardComponent;

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

/**
 * Delegate 테스트 용 액터 
 */
DECLARE_DELEGATE(FOnTakeDamage, float);

class ADelegateTestActor : public AActor
{
public:
    DECLARE_CLASS(ADelegateTestActor, AActor)
    ADelegateTestActor() = default;

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;

	void BeginPlay() override;

	void HandleTakeDamage(float InDamage);

	FOnTakeDamage OnTakeDamage;
};
