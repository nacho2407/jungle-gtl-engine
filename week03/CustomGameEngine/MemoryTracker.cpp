#include <new>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include "EngineCore/EngineStatics.h"

using uint32 = std::uint32_t;

struct AllocationHeader
{
	std::size_t Size;
};

void* operator new(std::size_t size)
{
	std::size_t totalSize = sizeof(AllocationHeader) + size;
	void* raw = std::malloc(totalSize);

	if (!raw)
		throw std::bad_alloc();

	auto* header = static_cast<AllocationHeader*>(raw);
	header->Size = size;

	UEngineStatics::TotalAllocatedBytes += static_cast<uint32>(size);
	UEngineStatics::TotalAllocationCount += 1;

	return reinterpret_cast<void*>(header + 1);
}

void operator delete(void* ptr) noexcept
{
	if (!ptr)
		return;

	auto* header = reinterpret_cast<AllocationHeader*>(ptr) - 1;

	UEngineStatics::TotalAllocatedBytes -= static_cast<uint32>(header->Size);
	UEngineStatics::TotalAllocationCount -= 1;

	std::free(header);
}

void* operator new[](std::size_t size)
{
	return ::operator new(size);
}

void operator delete[](void* ptr) noexcept
{
	::operator delete(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
	::operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
	::operator delete[](ptr);
}