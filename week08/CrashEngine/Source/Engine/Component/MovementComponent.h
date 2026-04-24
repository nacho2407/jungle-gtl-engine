// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/ActorComponent.h"

class USceneComponent;
class AActor;

// TODO : 해당 컴포넌트 베이스 역할을 하고 고유의 기능은 없기에 오브젝트에 부여할 수 없도록 바꿔야 합니다!

// UMovementComponent 컴포넌트이다.
class UMovementComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UMovementComponent, UActorComponent)

    UMovementComponent() = default;
    ~UMovementComponent() override = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
    USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }
    bool HasValidUpdatedComponent() const { return UpdatedComponent != nullptr; }

protected:
    void TryAutoRegisterUpdatedComponent();
    // Owner 또는 Outer 체인에서 AActor를 찾아 UpdatedComponentPath로 포인터를 복원한다.
    void ResolveUpdatedComponentByPath();

    USceneComponent* UpdatedComponent = nullptr; // 움직일 대상 (런타임 전용, 직렬화 제외)
    bool bAutoRegisterUpdatedComponent = true;
    // 루트에서의 계층 경로. 빈 문자열이면 bAutoRegisterUpdatedComponent 동작에 따름.
    //   "Root"  → 루트 컴포넌트
    //   "0"     → 루트의 첫 번째 자식
    //   "1/0"   → 루트의 두 번째 자식의 첫 번째 자식
    FString UpdatedComponentPath;
};
