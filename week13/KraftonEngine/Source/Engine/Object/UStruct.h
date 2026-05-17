#pragma once

#include "Core/CoreTypes.h"
#include "Core/PropertyTypes.h"
#include <cstring>

class UObject;

class UStruct
{
public:
	UStruct(const char* InName, UStruct* InSuperStruct, size_t InSize)
		: Name(InName), SuperStruct(InSuperStruct), Size(InSize)
	{
	}

	const char* GetName() const { return Name; }
	UStruct* GetSuperStruct() const { return SuperStruct; }
	size_t      GetSize() const { return Size; }

	bool IsChildOf(const UStruct* Other) const
	{
		for (const UStruct* S = this; S; S = S->SuperStruct)
		{
			if (S == Other)
			{
				return true;
			}
		}
		return false;
	}

	void AddProperty(const FProperty* Property)
	{
		if (!Property)
		{
			return;
		}

		for (const FProperty*& Existing : Properties)
		{
			const bool bSameName =
				Existing && Existing->Name && Property->Name && std::strcmp(Existing->Name, Property->Name) == 0;
			const bool bSameOwner =
				(Existing && !Existing->OwnerClassName && !Property->OwnerClassName)
				|| (Existing && Existing->OwnerClassName && Property->OwnerClassName && std::strcmp(Existing->OwnerClassName, Property->OwnerClassName) == 0);

			if (bSameName && bSameOwner)
			{
				Existing = Property;
				return;
			}
		}

		Properties.push_back(Property);
	}

	virtual void GetPropertyRefs(TArray<const FProperty*>& OutProperties, bool bIncludeSuper = true) const
	{
		if (bIncludeSuper && SuperStruct)
		{
			SuperStruct->GetPropertyRefs(OutProperties, true);
		}

		for (const FProperty* Prop : Properties)
		{
			if (Prop)
			{
				OutProperties.push_back(Prop);
			}
		}
	}

	static TArray<UStruct*>& GetAllStructs()
	{
		static TArray<UStruct*> Registry;
		return Registry;
	}

	static UStruct* FindStructByName(const char* InName)
	{
		if (!InName) return nullptr;
		for (UStruct* S : GetAllStructs())
		{
			if (S && S->GetName() && std::strcmp(S->GetName(), InName) == 0)
			{
				return S;
			}
		}
		return nullptr;
	}

private:
	const char* Name = nullptr;
	UStruct* SuperStruct = nullptr;
	size_t Size = 0;
	TArray<const FProperty*> Properties;
};

// static initializer 에서 UStruct를 전역 레지스트리에 등록
struct FStructRegistrar
{
	FStructRegistrar(UStruct* InStruct)
	{
		UStruct::GetAllStructs().push_back(InStruct);
	}
};
