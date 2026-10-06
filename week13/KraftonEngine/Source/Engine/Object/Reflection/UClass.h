#pragma once
#include "Object/Reflection/UStruct.h"


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

struct FPendingClassFlags
{
	const char* ClassName = nullptr;
	uint32      Flags     = CF_None;
};

class FClassFlagRegistry
{
public:
	static void AddClassFlags(const char* ClassName, uint32 Flags)
	{
		if (!ClassName || !ClassName[0] || Flags == CF_None)
		{
			return;
		}

		for (UClass* Class : UClass::GetAllClasses())
		{
			if (Class && Class->GetName() && std::strcmp(Class->GetName(), ClassName) == 0)
			{
				Class->AddClassFlags(Flags);
				return;
			}
		}

		GetPendingFlags().push_back(FPendingClassFlags { ClassName, Flags });
	}

	static void ApplyPendingFlags(UClass* Class)
	{
		if (!Class || !Class->GetName())
		{
			return;
		}

		TArray<FPendingClassFlags>& PendingFlags = GetPendingFlags();
		for (auto It = PendingFlags.begin(); It != PendingFlags.end(); )
		{
			const FPendingClassFlags& Pending = *It;
			if (Pending.ClassName && std::strcmp(Pending.ClassName, Class->GetName()) == 0)
			{
				Class->AddClassFlags(Pending.Flags);
				It = PendingFlags.erase(It);
				continue;
			}
			++It;
		}
	}

private:
	static TArray<FPendingClassFlags>& GetPendingFlags()
	{
		static TArray<FPendingClassFlags> PendingFlags;
		return PendingFlags;
	}
};


// static initializer 에서 UClass를 전역 레지스트리에 등록
struct FClassRegistrar
{
	FClassRegistrar(UClass* InClass)
	{
		UStruct::GetAllStructs().push_back(InClass);
		UClass::GetAllClasses().push_back(InClass);
		FClassFlagRegistry::ApplyPendingFlags(InClass);
	}
};
