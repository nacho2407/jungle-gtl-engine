#pragma once

#include "ObjectFactory.h"

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
				FObjectFactory::RegisterObjectType(cls::GetClass(), []() -> UObject* { return new cls(); }); \
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

	void* operator new(size_t size);
	void operator delete(void* p, size_t size);

public:
	uint32 UUID;
	uint32 InternalIndex;
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
