#include "FWorldSaveConverter.h"

#include "Actor/ASphere.h"
#include "Actor/ACube.h"
#include "Actor/ATriangle.h"

const TMap<FString, FString> FWorldSaveConverter::ClassNameMap =
{
    { "Sphere", "ASphere" },
    { "Cube", "ACube" },
    { "Triangle", "ATriangle" }
};

static FString FindSavedTypeByClassName(const FString& ClassName)
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

FWorldSaveData FWorldSaveConverter::FromWorld(const UWorld* World)
{
    FWorldSaveData SaveData;

    if (World == nullptr)
    {
        return SaveData;
    }

    SaveData.Version = 1;

    SaveData.NextUUID = UEngineStatics::NextUUID;

    const TArray<AActor*>& Actors = World->GetActors();

    for (const AActor* Actor : Actors)
    {
        SaveData.Primitives.push_back(MakePrimitiveRecord(Actor));
    }

    return SaveData;
}

bool FWorldSaveConverter::ToWorld(const FWorldSaveData& SaveData, UWorld* World)
{
    if (World == nullptr)
    {
        return false;
    }

    /*
     * ********** 이 위치에 World를 비우는 로직 추가 *************
     * ********** 예: World->ClearActors(); ******************
     */

    for (const FPrimitiveRecord& Record : SaveData.Primitives)
    {
        AActor* NewActor = MakeActorFromRecord(Record);
        if (NewActor == nullptr)
        {
            continue;
        }

        World->AddActor(NewActor);
    }

    // ----------------------------
    // [가정]
    // 저장된 NextUUID를 다시 엔진 전역 상태에 반영하는 인터페이스가 아직 확정되지 않았습니다.
    //
    // 실제로는 아래와 같은 형태 중 하나가 필요할 수 있습니다.
    //   UEngineStatics::SetNextUUID(SaveData.NextUUID);
    // ----------------------------

    return true;
}

FPrimitiveRecord FWorldSaveConverter::MakePrimitiveRecord(const AActor* Actor)
{
    FPrimitiveRecord Record;

    if (Actor == nullptr)
    {
        return Record;
    }

    // ----------------------------
    // [가정]
    // AActor가 클래스 이름을 문자열로 반환한다고 가정
    // ----------------------------
    const FString ClassName = Actor->GetClassName();

    FString SavedType = FindSavedTypeByClassName(ClassName);
    if (SavedType.IsEmpty())
    {
        // 매핑이 없으면 클래스 이름 자체를 저장
        SavedType = ClassName;
    }

    Record.Type = SavedType;

    // ----------------------------
    // [가정]
    // AActor의 Transform 접근 인터페이스를 아래처럼 가정했습니다.
    //
    //   FVector AActor::GetActorLocation() const;
    //   FVector AActor::GetActorRotation() const;   // Euler degree/vector 라고 가정
    //   FVector AActor::GetActorScale() const;
    //
    // 실제 프로젝트에서
    //   GetLocation(), GetRotation(), GetScale()
    // 혹은 RootComponent 기반 접근이라면 거기에 맞게 수정해 주세요.
    // ----------------------------
    Record.Location = Actor->GetActorLocation();
    Record.Rotation = Actor->GetActorRotation();
    Record.Scale = Actor->GetActorScale();

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
    else
    {
        // 알 수 없는 타입
        return nullptr;
    }

    // ----------------------------
    // [가정]
    // AActor의 Transform 설정 인터페이스를 아래처럼 가정했습니다.
    //
    //   void AActor::SetActorLocation(const FVector& InLocation);
    //   void AActor::SetActorRotation(const FVector& InRotation);
    //   void AActor::SetActorScale(const FVector& InScale);
    //
    // 실제 프로젝트에서 함수명이 다르면 그에 맞게 바꿔 주세요.
    // ----------------------------
    NewActor->SetActorLocation(Record.Location);
    NewActor->SetActorRotation(Record.Rotation);
    NewActor->SetActorScale(Record.Scale);

    return NewActor;
}