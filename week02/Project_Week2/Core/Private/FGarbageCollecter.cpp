#include "FGarbageCollector.h"
#include "CoreDefine.h"
#include "FUObjectAllocator.h"

//1턴 -> Outer 참조 그래프 생성 및 루트 탐색 및 flag 초기화
//2턴 -> 루트에서 부터 그래프 탐색으로 flag설정
//3턴 -> Sweep / Destroy

void FGarbageCollector::CheckUObjectArray()
{
    int32 memorySize = GUObjectArray.Num();
    TMap<int32, TArray<int32>> RefGraph;

    // 1. 초기화 + Outer 그래프 생성
    for (int32 i = 0; i < memorySize; i++)
    {
        FUObjectItem& CurrentItem = GUObjectArray[i];
        CurrentItem.bIsGCTarget = true;

        UObject* CurrentObject = CurrentItem.GetObject();
        if (!CurrentObject)
            continue;

        UObject* Outer = CurrentObject->GetOuter();
        if (!Outer)
            continue;

        int32 OuterIndex = Outer->InternalIndex;
        int32 CurrentIndex = CurrentObject->InternalIndex;

        if (OuterIndex == CurrentIndex)
            continue;

        RefGraph[OuterIndex].push_back(CurrentIndex);
    }

    // 2. 루트 찾기
    TDeque<int32> RootQueue;
    for (int32 i = 0; i < memorySize; i++)
    {
        FUObjectItem& Item = GUObjectArray[i];
        UObject* Obj = Item.GetObject();
        if (!Obj)
            continue;

        // 예시 정책:
        // Outer가 없으면 루트
        // 또는 별도 RootSet / Standalone / World / Package 등을 루트 취급
        if (Obj->GetOuter() == nullptr && !Item.bPendingKill)
        {
            Item.bIsGCTarget = false;
            RootQueue.push_back(Obj->InternalIndex);
        }
    }

    // 3. BFS Mark
    while (!RootQueue.empty())
    {
        int32 CurrentIndex = RootQueue.front();
        RootQueue.pop_front();

        FUObjectItem* CurrentItem = GUObjectArray.IndexToObject(CurrentIndex);
        if (!CurrentItem || CurrentItem->bPendingKill)
            continue;

        if (!RefGraph.contains(CurrentIndex))
            continue;

        TArray<int32>& Children = RefGraph[CurrentIndex];
        for (int32 ChildIndex : Children)
        {
            FUObjectItem* ChildItem = GUObjectArray.IndexToObject(ChildIndex);
            if (!ChildItem)
                continue;

            if (!ChildItem->bIsGCTarget)
                continue;

            ChildItem->bIsGCTarget = false;
            RootQueue.push_back(ChildIndex);
        }
    }

    // 4. Sweep
    for (int32 i = 0; i < memorySize; i++)
    {
        FUObjectItem& CurrentItem = GUObjectArray[i];
        UObject* TargetObject = CurrentItem.GetObject();

        if (!TargetObject)
            continue;

        if (!CurrentItem.bIsGCTarget && !CurrentItem.bPendingKill)
            continue;

        GUObjectArray.FreeUObjectIndox(TargetObject);
        GUObjectAllocator.FreeUObject(TargetObject);
    }
}