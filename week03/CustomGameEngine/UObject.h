#pragma once
#include "UEngineTypes.h"

class UObject
{
public:
	UObject();
	virtual ~UObject();

public:
	void* operator new(size_t size);
	void operator delete(void* p, size_t size);

public:
	uint32 UUID;
	uint32 InternalIndex;
};
