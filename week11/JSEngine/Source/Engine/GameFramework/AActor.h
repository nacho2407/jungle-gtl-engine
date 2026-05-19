#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Engine/GameFramework/WorldContext.h"
#include "Component/ShapeComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Core/Delegates/Delegate.h"
#include <type_traits>

// ★ 리플렉션 필수 !!
#include "AActor.generated.h"

class UWorld;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FAnimNotifyDispatchEvent;

UCLASS()
class AActor : public UObject
{
    GENERATED_BODY_AActor() //  UHT가 자동 생성한 뼈대 코드가 주입될 공간

public:
    // 참고: 나중에 GENERATED_BODY 내부로 이 기능(DECLARE_CLASS)을 통합할 수 있습니다.
    DECLARE_CLASS(AActor, UObject)
    AActor() = default;
    ~AActor() override;

    virtual void PostDuplicate(UObject* Original) override;

    virtual void Serialize(FArchive& Ar) override;

    virtual void InitDefaultComponents() {}

    FString MakeUniqueComponentName(const UActorComponent* TargetComponent, const FString& RequestedName, bool bAlwaysAppendNumber) const;

    // 컴포넌트 생성 + Owner 설정 + 등록만 수행. Attach는 별도로 호출할 것.
    template <typename T>
    T* AddComponent()
    {
        static_assert(std::is_base_of_v<UActorComponent, T>,
                      "AddComponent<T>: T must derive from UActorComponent");

        T* Comp = UObjectManager::Get().CreateObject<T>();

        bPrimitiveCacheDirty = true;

        Comp->SetOwner(this);
        Comp->SetFName(FName(MakeUniqueComponentName(Comp, T::s_TypeInfo.name, true)));
        OwnedComponents.push_back(Comp);
        bPrimitiveCacheDirty = true;
        NotifyComponentRegistered(Comp);
        return Comp;
    }

    // Tick 관련
    virtual void BeginPlay();
    virtual void Tick(float DeltaTime);
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

    bool IsActive() const { return bIsActive; }
    void SetActive(bool bEnabled) { bIsActive = bEnabled; }

    bool ShouldTickInEditor() const { return bTickInEditor; }
    void SetTickInEditor(bool bEnabled) { bTickInEditor = bEnabled; }

    // FTypeInfo 기반 런타임 컴포넌트 생성
    UActorComponent* AddComponentByClass(const FTypeInfo* Class);
    void RemoveComponent(UActorComponent* Component);
    void RegisterComponent(UActorComponent* Comp);

    void SetRootComponent(USceneComponent* Comp);
    USceneComponent* GetRootComponent() const { return RootComponent; }

    const TArray<UActorComponent*>& GetComponents() const { return OwnedComponents; }

    // 순차 탐색 후 가장 처음으로 일치하는 Component 반환
    template <typename T>
    T* FindComponent()
    {
        for (UActorComponent* Component : OwnedComponents)
        {
            if (Component->IsA<T>())
                return Cast<T>(Component);
        }

        return nullptr;
    }

    // Transform — Location
    FVector GetActorLocation() const;
    void SetActorLocation(const FVector& Location);

    void AddActorWorldOffset(const FVector& Delta)
    {
        if (RootComponent)
            RootComponent->AddWorldOffset(Delta);
    }

	// Transform — Rotation
	FVector GetActorRotation() const
	{
		return RootComponent ? RootComponent->GetRelativeRotation() : FVector(0, 0, 0);
	}
	void SetActorRotation(const FVector& NewRotation)
	{
		if (RootComponent) RootComponent->SetRelativeRotation(NewRotation);
	}
	void SetActorRotationQuat(const FQuat& Q)
	{
		if (RootComponent) RootComponent->SetRelativeRotationQuat(Q);
	}

    // Transform — Scale
    FVector GetActorScale() const
    {
        return RootComponent ? RootComponent->GetRelativeScale() : FVector(1, 1, 1);
    }
    void SetActorScale(const FVector& NewScale)
    {
        if (RootComponent)
            RootComponent->SetRelativeScale(NewScale);
    }

    // Direction
    FVector GetActorForward() const
    {
        if (RootComponent)
            return RootComponent->GetForwardVector();
        return FVector(0, 0, 1);
    }

    FVector GetActorRight() const
    {
        if (RootComponent)
            return RootComponent->GetRightVector();
        return FVector(0, 1, 0);
    }

    FVector GetActorUp() const
    {
        if (RootComponent)
            return RootComponent->GetUpVector();
        return FVector(0, 0, 1);
    }

    void SetWorld(UWorld* World);
    UWorld* GetFocusedWorld() const { return OwningWorld; }

    bool IsVisible() const { return bVisible; }
    void SetVisible(bool Visible);

    void AddTag(const FString& Tag);
    void RemoveTag(const FString& Tag);
    bool HasTag(const FString& Tag) const;
    void ClearTags();
    const TArray<FString>& GetTags() const { return Tags; }
    FString GetTagsText() const;
    void SetTagsFromText(const FString& InTagsText);

    // 프로퍼티 시스템 — UObject 에서 상속
    // 참고: 나중에 UHT가 속성을 자동 수집하게 되면 이 수동 함수들은 지울 수 있습니다.
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    const TArray<UPrimitiveComponent*>& GetPrimitiveComponents() const;

    bool IsOverlappingActor(const AActor* Other) const;

    virtual void PostComponentRegistered(UActorComponent* Comp);
    virtual void PostComponentUnregistered(UActorComponent* Comp);

    virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
    virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    virtual void HandleAnimNotify(USkeletalMeshComponent* SourceComponent, const FAnimNotifyDispatchEvent& NotifyEvent);

    void MarkPendingKill() { bPendingKill = true; }
    bool IsPendingKill() const { return bPendingKill; }

protected:
    void NotifyComponentRegistered(UActorComponent* Component);
    void NotifyComponentUnregistered(UActorComponent* Component);
    void MarkPrimitiveComponentsDirty();

    // ★ 추가 4: 변수들에 UPROPERTY 부착 (에디터 노출 및 가비지 컬렉터 추적 목적)

    UPROPERTY(EditAnywhere, Category="Actor", DisplayName="Root Component")
    USceneComponent* RootComponent = nullptr;

    UPROPERTY() // 에디터엔 안 보이지만 GC가 추적해야 하는 포인터
    UWorld* OwningWorld = nullptr;

    // Transform 값들은 보통 RootComponent를 통해 조작하므로 매크로 생략
    UPROPERTY(EditAnywhere, Category="Transform", DisplayName="Location")
    FVector PendingActorLocation = FVector(0, 0, 0);
    UPROPERTY(EditAnywhere, Category="Transform", DisplayName="Rotation")
    FVector PendingActorRotation = FVector(0, 0, 0);
    UPROPERTY(EditAnywhere, Category="Transform", DisplayName="Scale")
    FVector PendingActorScale = FVector(1, 1, 1);

    UPROPERTY(EditAnywhere, Category="Actor", DisplayName="Visible")
    bool bVisible = true;

    UPROPERTY(EditAnywhere, Category="Actor", DisplayName="Active")
    bool bIsActive = true;

    UPROPERTY(EditAnywhere, Category="Actor", DisplayName="Tick In Editor")
    bool bTickInEditor = false;

    UPROPERTY() // 배열 안에 포인터가 있으므로 GC 추적 필요
    TArray<UActorComponent*> OwnedComponents;

    UPROPERTY(EditAnywhere, Category="Actor", DisplayName="Tags")
    TArray<FString> Tags;

    // 렌더링용 캐시 및 엔진 내부 핸들 ID들은 에디터 노출/GC 추적이 필요 없으므로 매크로 생략
    mutable TArray<UPrimitiveComponent*> PrimitiveCache;
    mutable bool bPrimitiveCacheDirty = true;

    uint64 OnComponentBeginOverlapHandleId = 0;
    uint64 OnComponentEndOverlapHandleId = 0;
    uint64 OnComponentHitHandleId = 0;

    bool bPendingKill = false;
};
