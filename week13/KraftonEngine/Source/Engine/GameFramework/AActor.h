#pragma once
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Component/SceneComponent.h"
#include "Core/TickFunction.h"

class FArchive;

#include <type_traits>

enum ELevelTick : int;
struct FActorTickFunction;
class UWorld;
class ULevel;
class UPrimitiveComponent;

#include "Source/Engine/GameFramework/AActor.generated.h"

UCLASS()
class AActor : public UObject
{
    friend struct FActorTickFunction;
    friend class UActorComponent;
public:
	GENERATED_BODY()
	AActor();
	~AActor() override;

	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);
	virtual void EndPlay();
	virtual void RouteActorDestroyed();

	bool HasActorBegunPlay() const { return bActorHasBegunPlay; }

	void Serialize(FArchive& Ar) override;
	UObject* Duplicate(UObject* NewOuter = nullptr) const override;

	void PreGetEditableProperties() override;
	void PostEditProperty(const char* PropertyName) override;

	// 컴포넌트 생성 + Owner 설정 + 등록 + 렌더 상태 생성
	template<typename T>
	T* AddComponent() {
		static_assert(std::is_base_of_v<UActorComponent, T>,
			"AddComponent<T>: T must derive from UActorComponent");
		T* Comp = UObjectManager::Get().CreateObject<T>(this);
		RegisterComponent(Comp);
		return IsValid(Comp) && Comp->GetOwner() == this ? Comp : nullptr;
	}

	// UClass 기반 런타임 컴포넌트 생성
	UFUNCTION(Callable, Category="Actor|Components")
	UActorComponent* AddComponentByClass(UClass* Class);

	UFUNCTION(Callable, Category="Actor|Components")
	void RemoveComponent(UActorComponent* Component);

	// 외부에서 생성된 컴포넌트를 등록 (역직렬화 등)
	void RegisterComponent(UActorComponent* Comp);

	UFUNCTION(Callable, Category="Actor|Components")
	void SetRootComponent(USceneComponent* Comp);
	UFUNCTION(Pure, Category="Actor|Components")
	USceneComponent* GetRootComponent() const { return RootComponent.GetValid(); }

	TArray<UActorComponent*> GetComponents() const;

	// 특정 클래스의 컴포넌트를 검색하여 반환 (없으면 nullptr)
	template<typename T>
	T* GetComponentByClass() const {
		static_assert(std::is_base_of_v<UActorComponent, T>,
			"GetComponentByClass<T>: T must derive from UActorComponent");
		for (UActorComponent* Comp : OwnedComponents) {
			if (!IsValid(Comp)) {
				continue;
			}

			if (T* Casted = Cast<T>(Comp)) {
				return Casted;
			}
		}
		return nullptr;
	}

	// Transform — Location
	UFUNCTION(Pure, Category="Actor|Transform")
	FVector GetActorLocation() const;
	UFUNCTION(Callable, Category="Actor|Transform")
	void SetActorLocation(const FVector& Location);
	UFUNCTION(Callable, Category="Actor|Transform")
	void AddActorWorldOffset(const FVector& Delta);

	// Transform — Rotation
	UFUNCTION(Pure, Category="Actor|Transform")
	FRotator GetActorRotation() const;
	UFUNCTION(Callable, Category="Actor|Transform")
	void SetActorRotation(const FRotator& NewRotation);
	void SetActorRotation(const FVector& EulerRotation);

	// Transform — Scale
	UFUNCTION(Pure, Category="Actor|Transform")
	FVector GetActorScale() const;
	UFUNCTION(Callable, Category="Actor|Transform")
	void SetActorScale(const FVector& NewScale);

	// Direction
	UFUNCTION(Pure, Category="Actor|Transform")
	FVector GetActorForward() const;
	UFUNCTION(Pure, Category="Actor|Transform")
	FVector GetActorRight() const;

	UWorld* GetWorld() const;
	UWorld* GetWorldEvenIfPendingKill() const;
	ULevel* GetLevel() const;

	UFUNCTION(Pure, Category="Actor|Visibility")
	bool IsVisible() const { return bVisible; }
	UFUNCTION(Callable, Exec, Category="Actor|Visibility")
	void SetVisible(bool Visible);

	// Tags — 게임 측에서 액터를 의미적으로 분류하는 FName 배열 (UE Actor::Tags 대응).
	// Trigger volume 의 TriggerTag 같은 단일-태그 필드는 그대로 두고, 이건 범용 다중 태그.
	// 에디터에서는 콤마 구분 문자열로 편집 — PostEditProperty 가 split 해서 반영.
	UFUNCTION(Pure, Category="Actor|Tags")
	bool HasTag(const FName& Tag) const;
	UFUNCTION(Callable, Category="Actor|Tags")
	void AddTag(const FName& Tag);
	UFUNCTION(Callable, Category="Actor|Tags")
	void RemoveTag(const FName& Tag);
	const TArray<FName>& GetTags() const { return Tags; }
	void SetTags(TArray<FName> InTags) { Tags = std::move(InTags); }

	// Tick 필요 여부 — false면 Tick 호출 자체를 건너뜀 (StaticMesh 등)
	UPROPERTY(Edit, Save, Category="Header", DisplayName="Needs Tick")
	bool bNeedsTick = true;
	bool bTickInEditor = false;

	const TArray<UPrimitiveComponent*>& GetPrimitiveComponents() const;
	bool IsQueuedForPartitionUpdate() const { return bQueuedForPartitionUpdate; }
	void SetQueuedForPartitionUpdate(bool bQueued) { bQueuedForPartitionUpdate = bQueued; }

    void AddReferencedObjects(FReferenceCollector& Collector) override;
    void BeginDestroy() override;
	
	FActorTickFunction PrimaryActorTick;
protected:
	virtual void TickActor( float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction );
	
	void MarkPickingDirty();
	bool CanRegisterComponent(UActorComponent* Comp) const;
	bool OwnsComponent(const UActorComponent* Comp) const;
	void OnComponentBeingDestroyed(UActorComponent* Component);
	virtual void OnOwnedComponentRemoved(UActorComponent* Component);

	// Runtime ownership reference. SceneSaveManager serializes component topology explicitly, so this must stay Transient.
	UPROPERTY(Transient, Instanced, Category="Actor|Components")
	TObjectPtr<USceneComponent> RootComponent = nullptr;

	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Location")
	FVector PendingActorLocation = FVector(0, 0, 0);
	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Rotation")
	FRotator PendingActorRotation = FRotator(0, 0, 0);
	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Scale")
	FVector PendingActorScale = FVector(1, 1, 1);
	UPROPERTY(Edit, Save, Category="Header", DisplayName="Visible")
	bool PendingActorVisible = true;

	bool bVisible = true;

	TArray<FName> Tags;
	UPROPERTY(Edit, Save, Category="Header", DisplayName="Tags")
	FString PendingTagsString;  // 에디터용 — 콤마 구분 직렬화 캐시

	// Runtime-owned component list. Actor keeps components alive through AActor::AddReferencedObjects().
	UPROPERTY(Transient, Instanced, Category="Actor|Components")
	TArray<TObjectPtr<UActorComponent>> OwnedComponents;

	// 렌더링용 캐시
	mutable TArray<UPrimitiveComponent*> PrimitiveCache;
	mutable bool bPrimitiveCacheDirty = true;
	bool bQueuedForPartitionUpdate = false;
	bool bActorHasBegunPlay = false;
	bool bActorDestroyRouted = false;
};
