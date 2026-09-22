#pragma once

#include "EngineCore/EngineTypes.h"
#include "EngineCore/ObjectFactory.h"
#include "EngineCore/Name.h"
#define DECLARE_RTTI(cls, parent_cls) \
	public: \
		static const void* GetClass() { static int id = 0; return &id; } \
		virtual bool IsA(const void* id) const override \
		{ \
			 if (GetClass() == id) return true; \
			 return parent_cls::IsA(id); \
		}

#define REGISTER_CLASS(cls) \
	namespace { \
		struct cls##Register \
		{ \
			cls##Register() \
			{ \
				FObjectFactory::RegisterObjectType(cls::GetClass(), []() -> UObject* {\
					UObject* obj = new cls(); \
					obj->Name = FUniqueNameGenerator::MakeUniqueName(#cls); \
					return obj; }); \
			} \
		}; \
		static cls##Register global_##cls##Register; \
	}

class UObject
{
public:
	UObject();
	virtual ~UObject();

public:
	static const void* GetClass() { static int id = 0; return &id; }
	virtual bool IsA(const void* id) const { return GetClass() == id; }
	/**
	 * @brief UObject의 표시 이름을 안전하게 재할당합니다.
	 *
	 * @param InName 새로 설정할 기본 이름
	 */
	void SetName(const FString& InName);

public:
	uint32 UUID;
	uint32 InternalIndex;
	FName Name; // = FName("SimpleCUbe")
};

template <typename To, typename From>
To* Cast(From* obj)
{
	if (obj && obj->IsA(To::GetClass()))
	{
		return reinterpret_cast<To*>(obj);
	}
	return nullptr;
}


