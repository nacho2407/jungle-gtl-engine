#pragma once

#include "Component/ActorComponent.h"


#include "Source/Game/Component/CarGasComponent.generated.h"

UCLASS()
class UCarGasComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UCarGasComponent() = default;
	~UCarGasComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetGas(float Value);
	void AddGas(float Amount);
	bool ConsumeGas(float Amount);

	float GetGas() const { return Gas; }
	float GetMaxGas() const { return MaxGas; }
	float GetGasRatio() const;
	bool HasGas() const { return Gas > 0.0f; }

private:
	void ClampGas();

private:
	UPROPERTY(Edit, Save, Category="Car Gas", DisplayName="Gas", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float Gas = 100.0f;

	UPROPERTY(Edit, Save, Category="Car Gas", DisplayName="MaxGas", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float MaxGas = 100.0f;
};
