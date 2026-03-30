/*****************************************************************//**
 * \file   EditorGlobals.cpp
 * \brief  Engine 모듈과 new, delete overloading 짝 맞춰주기
 * 
 * \author jungle
 * \date   March 2026
 *********************************************************************/

#include "EditorGlobals.h"
#include "Memory/MemoryBase.h"
#include "Object/ObjectGlobals.h"

void* operator new(size_t Size)
{
	if (GetGMalloc())
	{
		return GetGMalloc()->Malloc(Size);
	}

	return std::malloc(Size);
}

void operator delete(void* Ptr) noexcept
{
	if (GetGMalloc())
		GetGMalloc()->Free(Ptr);
	else
		std::free(Ptr);
}

void* operator new[](size_t Size)
{
	if (GetGMalloc())
		return GetGMalloc()->Malloc(Size);

	return std::malloc(Size);
}

void operator delete[](void* Ptr) noexcept
{
	if (GetGMalloc())
		GetGMalloc()->Free(Ptr);
	else
		std::free(Ptr);
}
