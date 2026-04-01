#include "ObjectGlobals.h"
#include "Memory/MemoryBase.h"
#include "Object/Object.h"

ENGINE_API FMalloc* GetGMalloc()
{
	static FMalloc GMalloc;
	return &GMalloc;
}

void* operator new(size_t Size)
{
	UObject::LastNewSize = static_cast<uint32>(Size);
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
void operator delete(void* Ptr, size_t) noexcept
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
void operator delete[](void* Ptr, size_t) noexcept
{
	if (GetGMalloc())
		GetGMalloc()->Free(Ptr);
	else
		std::free(Ptr);
}