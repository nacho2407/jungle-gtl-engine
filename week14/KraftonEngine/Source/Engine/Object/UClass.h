#pragma once
#include "UStruct.h"


enum EClassFlags : uint32
{
	CF_None      = 0,
	CF_Actor     = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera    = 1 << 2,
	CF_HiddenInComponentList = 1 << 3,
};

class UClass : public UStruct
{
public:
	UClass(const char* InName, UClass* InSuperClass, size_t InSize, uint32 InFlags = CF_None)
		: UStruct(InName, InSuperClass, InSize), ClassFlags(InFlags)
	{}

	UClass*      GetSuperClass() const 
	{
		return static_cast<UClass*> (GetSuperStruct()); 
	}
	uint32       GetClassFlags() const { return ClassFlags; }
	void        AddClassFlags(uint32 Flags) { ClassFlags |= Flags; }

	bool IsA(const UClass* Other) const
	{
		for (const UClass* C = this; C; C = C->GetSuperClass())
		{
			if (C == Other)
				return true;
		}
		return false;
	}

	bool HasAnyClassFlags(uint32 Flags) const
	{
		return (ClassFlags & Flags) != 0;
	}

	// --- Global class registry ---
	static TArray<UClass*>& GetAllClasses()
	{
		static TArray<UClass*> Registry;
		return Registry;
	}

	// 이름으로 등록된 클래스 룩업. 못 찾으면 nullptr.
	// 직렬화/설정 파일에서 클래스를 string으로 지정할 때 사용.
	static UClass* FindByName(const char* InName)
	{
		if (!InName) return nullptr;
		for (UClass* C : GetAllClasses())
		{
			if (C && C->GetName() && std::strcmp(C->GetName(), InName) == 0)
				return C;
		}
		return nullptr;
	}

private:
	uint32      ClassFlags  = CF_None;
};


// static initializer 에서 UClass를 전역 레지스트리에 등록
struct FClassRegistrar
{
	FClassRegistrar(UClass* InClass)
	{
		UStruct::GetAllStructs().push_back(InClass);
		UClass::GetAllClasses().push_back(InClass);
	}
};
