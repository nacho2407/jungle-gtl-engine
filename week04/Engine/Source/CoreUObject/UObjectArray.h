#pragma once

#include "Core/CoreMinimal.h"

#include "CoreUObject/Object.h"

/**
 * @brief 현재는 Object만 들고 있는, ObjectArray를 위한 슬롯 아이템이지만 향후 GC 확장이나 ObjectPtr
 * 확장이 용이하도록 별도의 슬롯 아이템으로 나누어 두었습니다.
 */
struct ENGINE_API FUObjectItem
{
    UObject* Object = nullptr;
};

class ENGINE_API FUObjectArray
{
  public:
    /**
     * @brief Object를 슬롯 배열에 등록합니다.
     * 
     * @param Object 슬롯 배열에 등록할 Object 객체
     */
    void AllocateObjectIndex(UObject* Object);

    /**
     * @brief Object를 슬롯 배열에서 제거합니다.
     * 
     * @param Index 제거할 슬롯 배열 내 인덱스
     * 
     * @param Object 슬롯 배열에서 제거할 Object 객체. 안정적인 삭제를 위해 추가 정보를 받습니다
     */
    void FreeObjectIndex(uint32 Index, UObject* Object);

    const TArray<FUObjectItem>& GetObjectItemArray(void) { return Objects; }

    const FUObjectItem* GetObjectItem(uint32 Index) const;

    /**
     * @brief 살아있는 객체 수가 아니라 슬롯 배열 길이를 반환합니다. 배열 내에 비어있는 슬롯이 있을 수 있으니 nullptr 검사를 확실히 해야합니다.
     */
    uint32 Num() const { return Objects.size(); }
  private:
    TArray<FUObjectItem> Objects;
    TArray<uint32>      FreeIndices;
};
