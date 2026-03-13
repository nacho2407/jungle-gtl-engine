#pragma once
#include "UEngineTypes.h"

#define DECLARE_RTTI(cls, parent_cls) \
	public: \
		static const void* GetClass() { static int id = 0; return &id; } \
		virtual bool IsA(const void* id) const override \
		{ \
			 if (GetClass() == id) return true; \
			 return parent_cls::IsA(id); \
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
