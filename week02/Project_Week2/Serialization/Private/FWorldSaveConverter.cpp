#include "FWorldSaveConverter.h"

#include "Actor/ASphere.h"
#include "Actor/ACube.h"
#include "Actor/ATriangle.h"
#include "Actor/ATorus.h"

const TMap<FString, FString> FWorldSaveConverter::ClassNameMap =
{
    { "Sphere", "ASphere" },
    { "Cube", "ACube" },
    { "Triangle", "ATriangle" },
    { "Toruso", "AToruso" }
};

FWorldSaveData FWorldSaveConverter::FromWorld(const UWorld* World)
{
    FWorldSaveData SaveData;

    if (World == nullptr)
    {
        return SaveData;
    }

    SaveData.Version = 1;

    // 런타임 UUID를 저장하고 복원할 필요는 없기 때문에 과제 요구사항이 SaveID를 말하는 것이라 가정하고 설정
    // 영속성 있는 SaveID는 임시 ID긴 하지만 저장 파일 구조 내에서 Actor 간 부모-자식 관계 파악 등에 사용될 수 있음ㄴ
    uint32 SaveID = 0;
    const TArray<AActor*>& Actors = World->GetActors();
    for (SaveID = 0; SaveID < Actors.size(); SaveID++)
    {
        SaveData.Primitives.push_back(MakePrimitiveRecord(Actors[SaveID], SaveID));
    }

    SaveData.NextUUID = SaveID;

    return SaveData;
}

bool FWorldSaveConverter::ToWorld(const FWorldSaveData& SaveData, UWorld* World)
{
    if (World == nullptr)
    {
        return false;
    }

    World->Clear();

    for (const FPrimitiveRecord& Record : SaveData.Primitives)
    {
        AActor* NewActor = MakeActorFromRecord(Record);
        if (NewActor == nullptr)
        {
            continue;
        }

        World->AddActor(NewActor);
    }

    return true;
}

FPrimitiveRecord FWorldSaveConverter::MakePrimitiveRecord(const AActor* Actor, uint32 SaveID)
{
    FPrimitiveRecord Record;

    if (Actor == nullptr)
    {
        return Record;
    }

    const FString ClassName = Actor->GetClass()->ClassName;

    FString SavedType = FindSavedTypeByClassName(ClassName);
    if (SavedType.empty())
    {
        // 매핑이 없으면 클래스 이름 자체를 저장
        SavedType = ClassName;
    }

    Record.Type = SavedType;

    Record.Location = Actor->GetRootComponent()->GetRelativeLocation();
    Record.Rotation = Actor->GetRootComponent()->GetRelativeRotation();
    Record.Scale = Actor->GetRootComponent()->GetRelativeScale();

    return Record;
}

AActor* FWorldSaveConverter::MakeActorFromRecord(const FPrimitiveRecord& Record)
{
    AActor* NewActor = nullptr;

    if (Record.Type == "Sphere")
    {
        NewActor = new ASphere();
    }
    else if (Record.Type == "Cube")
    {
        NewActor = new ACube();
    }
    else if (Record.Type == "Triangle")
    {
        NewActor = new ATriangle();
    }
    else if (Record.Type == "Torus")
    {
        NewActor = new ATorus();
    }
    else
    {
        // 알 수 없는 타입
        return nullptr;
    }

    NewActor->GetRootComponent()->SetRelativeLocation(Record.Location);
    NewActor->GetRootComponent()->SetRelativeRotation(Record.Rotation);
    NewActor->GetRootComponent()->SetRelativeScale(Record.Scale);

    return NewActor;
}

FString FWorldSaveConverter::FindSavedTypeByClassName(const FString& ClassName)
{
    for (const auto& Pair : FWorldSaveConverter::ClassNameMap)
    {
        if (Pair.second == ClassName)
        {
            return Pair.first;
        }
    }

    return FString();
}
