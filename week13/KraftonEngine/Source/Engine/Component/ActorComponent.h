#pragma once

#include "Object/Object.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Core/TickFunction.h"

#include "Source/Engine/Component/ActorComponent.generated.h"

class AActor;
class UWorld;
class FScene;

UCLASS()
class UActorComponent : public UObject
{
    friend struct FActorComponentTickFunction;
	friend class AActor;

public:
	GENERATED_BODY()

	virtual void BeginPlay();
	virtual void EndPlay() {};
	virtual void RouteComponentDestroyed();

	// --- 렌더 상태 관리 (UE RegisterComponent/UnregisterComponent 대응) ---
	// 컴포넌트 등록 시 호출 — PrimitiveComponent에서 SceneProxy 생성
	virtual void CreateRenderState() {}
	// 컴포넌트 해제 시 호출 — PrimitiveComponent에서 SceneProxy 파괴
	virtual void DestroyRenderState() {}

	UFUNCTION(Callable, Category="Component|Activation")
	virtual void Activate();
	UFUNCTION(Callable, Category="Component|Activation")
	virtual void Deactivate();

	// --- Editor Only ---
	// 에디터 전용 컴포넌트: PIE/Game 월드에서 렌더링 비활성화
	UFUNCTION(Callable, Category="Component|Editor")
	void SetEditorOnly(bool bInEditorOnly);
	UFUNCTION(Pure, Category="Component|Editor")
	bool IsEditorOnly() const { return bEditorOnly; }
	bool IsEditorOnlyComponent() const { return IsEditorOnly(); }
	void SetEditorOnlyComponent(bool bInEditorOnly) { SetEditorOnly(bInEditorOnly); }

	bool IsHiddenInComponentTree() const { return bHiddenInComponentTree; }
	void SetHiddenInComponentTree(bool bHidden) { bHiddenInComponentTree = bHidden; }

	UFUNCTION(Callable, Exec, Category="Component|Activation")
	void SetActive(bool bNewActive);
	UFUNCTION(Callable, Category="Component|Activation")
	inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
	UFUNCTION(Callable, Category="Component|Tick")
	inline void SetComponentTickEnabled(bool bEnabled) {
		PrimaryComponentTick.SetTickEnabled(bEnabled);
	}
	virtual void Serialize(FArchive& Ar) override;

	UFUNCTION(Pure, Category="Component|Activation")
	inline bool IsActive() { return bIsActive; }

	void SetOwner(AActor* Actor);
	AActor* GetOwner() const { return Owner.Get(); }
	AActor* GetOwnerEvenIfPendingKill() const { return Owner.GetEvenIfPendingKill(); }
	UWorld* GetWorld() const;
	UWorld* GetWorldEvenIfPendingKill() const;

	// 프로퍼티 값 변경 후 호출. 하위 클래스에서 override하여 부수효과(리소스 재로딩 등) 처리.
	virtual void PostEditProperty(const char* PropertyName) override;
	// 선택된 프록시의 소유 액터 컴포넌트가 디버그 시각화를 FScene에 기여
	// FPrimitiveSceneProxy::CollectSelectedVisuals 에서 호출됨
	virtual void ContributeSelectedVisuals(FScene& Scene) const { (void)Scene; }
	
	FActorComponentTickFunction PrimaryComponentTick;

    void BeginDestroy() override;
    void AddReferencedObjects(FReferenceCollector& Collector) override;

protected:
	// Component의 Tick은 UE 기준 Actor가 아닌 별도 시스템에서 돌아가나, 현재 관리를 위해 friend AActor로 설정. 추후 시스템이 완성되면 별도 매니저에서 관리하도록 리팩토링할 예정.
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction);
	
	// Non-owning back-reference. Actor owns this component through AActor::OwnedComponents.
	TWeakObjectPtr<AActor> Owner;
	UPROPERTY(Edit, Save, Category="Header", DisplayName="bTickEnable")
	bool bTickEnable = true;
	bool bComponentDestroyRouted = false;

private:
	UPROPERTY(Edit, Save, Category="Header", DisplayName="bEditorOnly")
	bool bEditorOnly = false;
	UPROPERTY(Edit, Save, Category="Header", DisplayName="bIsActive")
	bool bIsActive = true;
	UPROPERTY(Edit, Save, Category="Header", DisplayName="bAutoActivate")
	bool bAutoActivate = true;
	UPROPERTY(Save, Category="Header", DisplayName="Hidden In Component Tree")
	bool bHiddenInComponentTree = false;
};
