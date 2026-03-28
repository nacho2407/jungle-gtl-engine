#pragma once

#include "Core/CoreMinimal.h"

#include "CoreUObject/Object.h"
#include "CoreUObject/ObjectItem.h"

class FUObjectArray
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

    const TArray<FObjectItem>& GetObjectItemArray(void) { return Objects; }

    const FObjectItem* GetObjectItem(uint32 Index) const;

    /**
     * @brief 살아있는 객체 수가 아니라 슬롯 배열 길이를 반환합니다. 배열 내에 비어있는 슬롯이 있을 수 있으니 nullptr 검사를 확실히 해야합니다.
     */
    uint32 Num() const { return Objects.size(); }
  private:
    TArray<FObjectItem> Objects;
    TArray<uint32>      FreeIndices;
};
