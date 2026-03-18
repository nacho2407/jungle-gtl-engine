#include "FUObjectArray.h"


FChunckedFixedObjectArray::FChunckedFixedObjectArray()
    : Objects(nullptr)
    , PreAllocatedObjects(nullptr)
    , MaxElements(0)
    , NumElements(0)
    , MaxChunks(0)
    , NumChunks(0)
{
}

FChunckedFixedObjectArray::~FChunckedFixedObjectArray()
{
    if (Objects)
    {
        for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
        {
            delete[] Objects[ChunkIndex];
            Objects[ChunkIndex] = nullptr;
        }

        delete[] Objects;
        Objects = nullptr;
    }
}

void FChunckedFixedObjectArray::ExpandChunksToIndex(int32 Index)
{
    check(Index >= 0 && Index < MaxElements);
    check(Objects != nullptr);

    const int32 ChunkIndex = Index / NumElementsPerChunk;

    while (ChunkIndex >= NumChunks)
    {
        check(NumChunks < MaxChunks);

        FUObjectItem* NewChunk = new FUObjectItem[NumElementsPerChunk];

        // 청크 내부를 안전하게 초기화
        FMemory::Memzero(NewChunk, sizeof(FUObjectItem) * NumElementsPerChunk);

        // 핵심: 반드시 슬롯에 넣어야 함
        Objects[NumChunks] = NewChunk;
        ++NumChunks;
    }

    check(ChunkIndex < NumChunks);
    check(Objects[ChunkIndex] != nullptr);
}

void FChunckedFixedObjectArray::PreAllocate(int32 InMaxElements)
{
    check(!Objects);
    check(InMaxElements > 0);

    MaxChunks = InMaxElements / NumElementsPerChunk + 1;
    MaxElements = MaxChunks * NumElementsPerChunk;

    Objects = new FUObjectItem * [MaxChunks];
    check(Objects != nullptr);

    check(Objects != nullptr)
    // 포인터 배열을 nullptr로 초기화
    FMemory::Memzero(Objects, sizeof(FUObjectItem*) * MaxChunks);

    for (int index = 0; index < MaxChunks; index++)
    {
        FUObjectItem* NewChunk = new FUObjectItem[NumElementsPerChunk];

        // 청크 내부를 안전하게 초기화
        FMemory::Memzero(NewChunk, sizeof(FUObjectItem) * NumElementsPerChunk);
        Objects[index] = NewChunk;
    }
    check(Objects != nullptr)
    NumChunks = 0;
    NumElements = 0;
}

int32 FChunckedFixedObjectArray::Num() const
{
    return NumElements;
}

int32 FChunckedFixedObjectArray::Capacity() const
{
    return MaxElements;
}

int32 FChunckedFixedObjectArray::IsValidIndex(int32 Index)
{
    return (Index >= 0 && Index <= Num());
}

FUObjectItem* FChunckedFixedObjectArray::GetObjectPtr(int32 Index)
{
    check(Index >= 0)
    check(Objects != nullptr);

    const uint32 ChunkIndex = (uint32)Index / NumElementsPerChunk;
    const uint32 WithinChunkIndex = (uint32)Index % NumElementsPerChunk;

    check(Objects != nullptr);
    FUObjectItem* Chunk = *(Objects + ChunkIndex);
    check(Objects != nullptr);

    check(Chunk != nullptr);

    return Chunk + WithinChunkIndex;
}

FUObjectItem const* FChunckedFixedObjectArray::GetObjectPtr(int32 Index) const
{
    check(Index >= 0 && Index < NumElements);
    check(Objects != nullptr);

    const uint32 ChunkIndex = (uint32)Index / NumElementsPerChunk;
    const uint32 WithinChunkIndex = (uint32)Index % NumElementsPerChunk;

    FUObjectItem* Chunk = Objects[ChunkIndex];
    check(Chunk != nullptr);

    return Chunk + WithinChunkIndex;
}

int32 FChunckedFixedObjectArray::AddRange(int32 NumToAdd)
{
    check(NumToAdd > 0);
    check(NumElements + NumToAdd <= MaxElements);

    const int32 Result = NumElements;

    ExpandChunksToIndex(Result + NumToAdd - 1);

    NumElements += NumToAdd;
    return Result;
}

int32 FChunckedFixedObjectArray::AddSingle()
{
    return AddRange(1);
}

UObject* FUObjectItem::GetObject()
{
    return Object;
}

void FUObjectArray::AllocatdUObjectIndex(UObject* Object, int32 SerialNumber)
{
    check(Object != nullptr);

    int32 Index = -1;

    // 새 객체라면 아직 인덱스 없어야 자연스러움
    check(Object->InternalIndex >= 0);

    if (!ObjAvailableList.empty())
    {
        Index = ObjAvailableList.back();
        ObjAvailableList.pop_back();

        const int32 AvailableCount = (int32)ObjAvailableList.size();
        check(AvailableCount >= 0);
        ObjAvailableListEstimateCount = AvailableCount;
    }
    else
    {
        Index = Objects.AddSingle();
    }

    FUObjectItem* ObjectItem = IndexToObject(Index);
    check(ObjectItem != nullptr);

    ObjectItem->SetObject(Object);
    ObjectItem->ClusterRootIndex = 0;
    ObjectItem->SerialNumber = SerialNumber;

    Object->InternalIndex = Index;
    ElementalCount++;
}

void FUObjectArray::FreeUObjectIndox(UObject* Object)
{
    check(Object != nullptr);
    check(Object->InternalIndex >= 0);

    const int32 Index = Object->InternalIndex;
    FUObjectItem* ObjectItem = IndexToObject(Index);
    check(ObjectItem != nullptr);

    ObjectItem->SetObject(nullptr);
    ObjectItem->ClusterRootIndex = 0;
    ObjectItem->bPendingKill = false;
    ObjectItem->SerialNumber = 0;

    Object->InternalIndex = -1;

    ObjAvailableList.push_back(Index);
    ObjAvailableListEstimateCount = (int32)ObjAvailableList.size();
    ElementalCount--;
}

int32 FUObjectArray::AllocateSerialNumber(int32 Index)
{
    FUObjectItem* ObjectItem = IndexToObject(Index);

    if (!ObjectItem)
    {
        return 0;
    }

    return ObjectItem->SerialNumber++;
}