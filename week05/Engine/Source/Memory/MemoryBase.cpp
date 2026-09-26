#include "MemoryBase.h"
#include <memory>

void* FMalloc::Malloc(SIZE_T Count)
{
	/** malloc(0) 의 경우 플랫폼마다 동작이 다름 */
	if (Count == 0)
		Count = 1;

	/** Free 에서 Size 정보를 얻기 위함 */
	/** Memory: [Header] [          Data             ]  */
	SIZE_T        TotalSize = Count + HEADER_STRIDE;

	// Overflow
	if (TotalSize < Count)
		abort();

	void* Raw = std::malloc(TotalSize);
	if (!Raw)
		abort(); // OOM

	FAllocHeader* Header = (FAllocHeader*)Raw;
	Header->MagicNumber = MALLOC_MAGIC;
	Header->Size = Count;

	MallocStats.TotalAllocationCount++;
	MallocStats.CurrentAllocationCount++;
	MallocStats.TotalAllocationBytes += Count;
	MallocStats.CurrentAllocationBytes += Count;

	return (uint8*)Raw + HEADER_STRIDE;
}

void FMalloc::Free(void* Original)
{
	if (!Original)
		return;

	FAllocHeader* Header = (FAllocHeader*)((uint8*)Original - HEADER_STRIDE);
	if (Header->MagicNumber == 0)
	{
		return;
	}
	// Magic 검사 — GMalloc 이전 일반 할당이거나 double free, 오염된 포인터
	if (Header->MagicNumber != MALLOC_MAGIC)
	{
		// GMalloc 이전 일반 malloc 블록이면 그냥 해제
		std::free(Original);
		return;
	}

	// double free 방지 — 해제 전에 Magic 무효화
	Header->MagicNumber = 0;

	if (MallocStats.CurrentAllocationCount > 0)
		MallocStats.CurrentAllocationCount--;
	if (MallocStats.CurrentAllocationBytes >= Header->Size)
		MallocStats.CurrentAllocationBytes -= Header->Size;

	std::free(Header);
}