#include "Game/Component/CarGasComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

void UCarGasComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	ClampGas();
}

void UCarGasComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Gas") == 0 || std::strcmp(PropertyName, "MaxGas") == 0)
	{
		ClampGas();
	}
}

void UCarGasComponent::SetGas(float Value)
{
	Gas = Value;
	ClampGas();
}

void UCarGasComponent::AddGas(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	SetGas(Gas + Amount);
}

bool UCarGasComponent::ConsumeGas(float Amount)
{
	if (Amount <= 0.0f)
	{
		return true;
	}

	if (Gas < Amount)
	{
		Gas = 0.0f;
		return false;
	}

	Gas -= Amount;
	return true;
}

float UCarGasComponent::GetGasRatio() const
{
	return MaxGas > 0.0f ? Gas / MaxGas : 0.0f;
}

void UCarGasComponent::ClampGas()
{
	MaxGas = std::max(MaxGas, 0.0f);
	Gas = std::clamp(Gas, 0.0f, MaxGas);
}
