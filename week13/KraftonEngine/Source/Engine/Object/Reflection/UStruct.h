#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/PropertyTypes.h"
#include <cstring>

class UObject;

class UStruct
{
public:
	UStruct(const char* InName, UStruct* InSuperStruct, size_t InSize)
		: Name(InName), SuperStruct(InSuperStruct), Size(InSize)
	{
	}

	UStruct(const UStruct&) = delete;
	UStruct& operator=(const UStruct&) = delete;
	UStruct(UStruct&&) = delete;
	UStruct& operator=(UStruct&&) = delete;

	virtual ~UStruct()
	{
		for (FProperty* Property : Properties)
		{
			delete Property;
		}
		Properties.clear();

		for (FFunction* Function : Functions)
		{
			delete Function;
		}
		Functions.clear();
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

	void AddFunction(FFunction* Function)
	{
		if (!Function)
		{
			return;
		}

		for (FFunction*& Existing : Functions)
		{
			const bool bSameSignature = Existing && Existing->GetSignature() && Function->GetSignature() && std::strcmp(
				Existing->GetSignature(),
				Function->GetSignature()
			) == 0;
			const bool bSameOwner = (Existing && !Existing->OwnerClassName && !Function->OwnerClassName) || (Existing &&
				Existing->OwnerClassName && Function->OwnerClassName && std::strcmp(
					Existing->OwnerClassName,
					Function->OwnerClassName
				) == 0);

			if (bSameSignature && bSameOwner)
			{
				if (Existing != Function)
				{
					delete Existing;
				}
				Existing = Function;
				return;
			}
		}

		Functions.push_back(Function);
	}

	void AddProperty(FProperty* Property)
	{
		if (!Property)
		{
			return;
		}

		for (FProperty*& Existing : Properties)
		{
			const bool bSameName =
				Existing && Existing->Name && Property->Name && std::strcmp(Existing->Name, Property->Name) == 0;
			const bool bSameOwner =
				(Existing && !Existing->OwnerClassName && !Property->OwnerClassName)
				|| (Existing && Existing->OwnerClassName && Property->OwnerClassName && std::strcmp(Existing->OwnerClassName, Property->OwnerClassName) == 0);

			if (bSameName && bSameOwner)
			{
				if (Existing != Property)
				{
					delete Existing;
				}
				Existing = Property;
				InvalidateReferenceTokenStream();
				return;
			}
		}

		Properties.push_back(Property);
		InvalidateReferenceTokenStream();
	}

	void InvalidateReferenceTokenStream() const
	{
		if (bReferenceTokenStreamDirty)
		{
			return;
		}

		bReferenceTokenStreamDirty = true;

		for (UStruct* Struct : GetAllStructs())
		{
			if (Struct && Struct->GetSuperStruct() == this)
			{
				Struct->InvalidateReferenceTokenStream();
			}
		}
	}

	bool HasObjectReferences() const
	{
		return !GetReferenceTokenStream().empty();
	}

	const TArray<FGCReferenceToken>& GetReferenceTokenStream() const
	{
		if (bReferenceTokenStreamDirty)
		{
			BuildReferenceTokenStream();
		}
		return ReferenceTokenStream;
	}

	virtual void GetFunctionRefs(TArray<const FFunction*>& OutFunctions, bool bIncludeSuper = true) const
	{
		if (bIncludeSuper && SuperStruct)
		{
			SuperStruct->GetFunctionRefs(OutFunctions, true);
		}

		for (const FFunction* Function : Functions)
		{
			if (Function)
			{
				OutFunctions.push_back(Function);
			}
		}
	}

	const FFunction* FindFunctionByName(const char* InName, bool bIncludeSuper = true) const
	{
		if (!InName)
		{
			return nullptr;
		}

		for (const FFunction* Function : Functions)
		{
			if (Function && Function->Name && std::strcmp(Function->Name, InName) == 0)
			{
				return Function;
			}
		}

		return bIncludeSuper && SuperStruct ? SuperStruct->FindFunctionByName(InName, true) : nullptr;
	}

	void FindFunctionsByName(
		const char*               InName,
		TArray<const FFunction*>& OutFunctions,
		bool                      bIncludeSuper = true
		) const
	{
		if (!InName)
		{
			return;
		}

		if (bIncludeSuper && SuperStruct)
		{
			SuperStruct->FindFunctionsByName(InName, OutFunctions, true);
		}

		for (const FFunction* Function : Functions)
		{
			if (Function && Function->Name && std::strcmp(Function->Name, InName) == 0)
			{
				OutFunctions.push_back(Function);
			}
		}
	}

	const FFunction* FindFunctionBySignature(const char* InSignature, bool bIncludeSuper = true) const
	{
		if (!InSignature)
		{
			return nullptr;
		}

		for (const FFunction* Function : Functions)
		{
			if (Function && Function->GetSignature() && std::strcmp(Function->GetSignature(), InSignature) == 0)
			{
				return Function;
			}
		}

		return bIncludeSuper && SuperStruct ? SuperStruct->FindFunctionBySignature(InSignature, true) : nullptr;
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
	void BuildReferenceTokenStream() const
	{
		ReferenceTokenStream.clear();

		if (SuperStruct)
		{
			const TArray<FGCReferenceToken>& SuperTokens = SuperStruct->GetReferenceTokenStream();
			ReferenceTokenStream.insert(ReferenceTokenStream.end(), SuperTokens.begin(), SuperTokens.end());
		}

		for (const FProperty* Property : Properties)
		{
			if (!Property || !Property->ContainsObjectReference())
			{
				continue;
			}

			FGCReferenceToken Token;
			Token.Property = Property;
			Token.Type = Property->GetReferenceTokenType();
			ReferenceTokenStream.push_back(Token);
		}

		bReferenceTokenStreamDirty = false;
	}

private:
	const char*        Name        = nullptr;
	UStruct*           SuperStruct = nullptr;
	size_t             Size        = 0;
	TArray<FProperty*> Properties;
	TArray<FFunction*> Functions;
	mutable TArray<FGCReferenceToken> ReferenceTokenStream;
	mutable bool bReferenceTokenStreamDirty = true;
};

// static initializer 에서 UStruct를 전역 레지스트리에 등록
struct FStructRegistrar
{
	FStructRegistrar(UStruct* InStruct)
	{
		UStruct::GetAllStructs().push_back(InStruct);
	}
};
