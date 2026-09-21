#include "FGarbageCollector.h"
#include "CoreDefine.h"
#include "FUObjectAllocator.h"

//1턴 -> Outer 참조 그래프 생성 및 루트 탐색 및 flag 초기화
//2턴 -> 루트에서 부터 그래프 탐색으로 flag설정
//3턴 -> Sweep / Destroy

void FGarbageCollector::CollectGarbage()
{
    int32 memorySize = GUObjectArray.Num();
    TMap<int32, TArray<int32>> RefGraph;

    for (int32 i = 0; i < memorySize; i++)
    {
        FUObjectItem& CurrentItem = GUObjectArray[i];
        CurrentItem.bIsGCTarget = true;

        UObject* CurrentObject = CurrentItem.GetItemObject();
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

    TDeque<int32> RootQueue;
    for (int32 i = 0; i < memorySize; i++)
    {
        FUObjectItem& Item = GUObjectArray[i];
        UObject* Obj = Item.GetItemObject();
        if (!Obj)
            continue;

        if (Obj->GetOuter() == nullptr && !Item.bPendingKill)
        {
            Item.bIsGCTarget = false;
            RootQueue.push_back(Obj->InternalIndex);
        }
    }

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
        UObject* TargetObject = CurrentItem.GetItemObject();

        if (!TargetObject)
            continue;

        if (!CurrentItem.bIsGCTarget && !CurrentItem.bPendingKill)
            continue;

        GUObjectArray.FreeUObjectIndox(TargetObject);
        GUObjectAllocator.FreeUObject(TargetObject);
    }
}