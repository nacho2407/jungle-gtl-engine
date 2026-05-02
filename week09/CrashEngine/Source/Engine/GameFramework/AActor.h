// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Core/Delegates/Delegate.h"
#include "Core/TickFunction.h"

class FArchive;

#include <type_traits>

enum ELevelTick;
struct FActorTickFunction;
class UWorld;
class ULevel;
class UPrimitiveComponent;

// AActor는 월드에 배치되는 게임 오브젝트를 표현합니다.
class AActor : public UObject
{
    friend struct FActorTickFunction;

public:
    DECLARE_CLASS(AActor, UObject)
    AActor();
    ~AActor() override;

    virtual void BeginPlay();
    virtual void Tick(float DeltaTime);
    virtual void EndPlay();
    // Start ----- Delegate Prototype 
    virtual void TakeDamage(int Damage);

    DECLARE_DELEGATE(FOnTakeDamage, int);
    FOnTakeDamage OnTakeDamage;
    // End ----- Delegate Prototype

    bool HasActorBegunPlay() const { return bActorHasBegunPlay; }

    void Serialize(FArchive& Ar) override;
    UObject* Duplicate(UObject* NewOuter = nullptr) const override;

    // 컴포넌트 생성 + Owner 설정 + 등록 + 렌더 상태 생성
    template <typename T>
    T* AddComponent()
    {
        static_assert(std::is_base_of_v<UActorComponent, T>,
                      "AddComponent<T>: T must derive from UActorComponent");
        // 템플릿 경로도 abstract 베이스 컴포넌트 직접 추가를 막는다.
        if (T::StaticClass()->HasAnyClassFlags(CF_Abstract))
        {
            return nullptr;
        }

        T* Comp = UObjectManager::Get().CreateObject<T>(this);
        Comp->SetOwner(this);
        OwnedComponents.push_back(Comp);
        bPrimitiveCacheDirty = true;
        Comp->CreateRenderState();
        MarkPickingDirty();
        return Comp;
    }

    // UClass 기반 런타임 컴포넌트 생성
    UActorComponent* AddComponentByClass(UClass* Class);

    void RemoveComponent(UActorComponent* Component);

    // 외부에서 생성된 컴포넌트를 등록 (역직렬화 등)
    void RegisterComponent(UActorComponent* Comp);

    void SetRootComponent(USceneComponent* Comp);
    USceneComponent* GetRootComponent() const { return RootComponent; }

    const TArray<UActorComponent*>& GetComponents() const { return OwnedComponents; }

    // Transform — Location
    FVector GetActorLocation() const;
    void SetActorLocation(const FVector& Location);
    void AddActorWorldOffset(const FVector& Delta);

    // Transform — Rotation
    FRotator GetActorRotation() const;
    void SetActorRotation(const FRotator& NewRotation);
    void SetActorRotation(const FVector& EulerRotation);

    // Transform — Scale
    FVector GetActorScale() const;
    void SetActorScale(const FVector& NewScale);

    // Direction
    FVector GetActorForward() const;

    UWorld* GetWorld() const;
    ULevel* GetLevel() const;

    bool IsVisible() const { return bVisible; }
    void SetVisible(bool Visible);

	bool IsActorTickEnabled() const { return bNeedsTick; }
    void SetActorTickEnabled(bool bEnabled);

    const TArray<UPrimitiveComponent*>& GetPrimitiveComponents() const;
    bool IsQueuedForPartitionUpdate() const { return bQueuedForPartitionUpdate; }
    void SetQueuedForPartitionUpdate(bool bQueued) { bQueuedForPartitionUpdate = bQueued; }

public:
    // Tick 필요 여부 — false면 Tick 호출 자체를 건너뜀 (StaticMesh 등)
    bool bNeedsTick = true;
    bool bTickInEditor = false;

    FActorTickFunction PrimaryActorTick;

	//충돌 처리
    bool IsOverlappingActor(const AActor* Other) const;

protected:
    virtual void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction);

    void MarkPickingDirty();
    void EnsureDefaultEditorHelperTextComponent();

    USceneComponent* RootComponent = nullptr;

    FVector PendingActorLocation = FVector(0, 0, 0);
    bool bVisible = true;

    TArray<UActorComponent*> OwnedComponents;

    // 렌더링용 캐시
    mutable TArray<UPrimitiveComponent*> PrimitiveCache;
    mutable bool bPrimitiveCacheDirty = true;
    bool bQueuedForPartitionUpdate = false;
    bool bActorHasBegunPlay = false;
    bool bSuppressDefaultEditorHelperCreation = false;
};
