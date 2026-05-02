#pragma once

#include "Component/ActorComponent.h"

class UCarGasComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UCarGasComponent, UActorComponent)

	UCarGasComponent() = default;
	~UCarGasComponent() override = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
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
	float Gas = 100.0f;
	float MaxGas = 100.0f;
};
