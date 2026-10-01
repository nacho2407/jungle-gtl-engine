// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/Object.h"
#include "Core/PropertyTypes.h"
#include "Core/TickFunction.h"

class AActor;
class UWorld;
class FScene;

// UActorComponent 컴포넌트이다.
class UActorComponent : public UObject
{
    friend struct FActorComponentTickFunction;
    friend class AActor;

public:
    DECLARE_CLASS(UActorComponent, UObject)

    virtual void BeginPlay();
    virtual void EndPlay() {};

    // --- 렌더 상태 관리 (UE RegisterComponent/UnregisterComponent 대응) ---
    // 컴포넌트 등록 시 호출 ? PrimitiveComponent에서 SceneProxy 생성
    virtual void CreateRenderState() {}
    // 컴포넌트 해제 시 호출 ? PrimitiveComponent에서 SceneProxy 파괴
    virtual void DestroyRenderState() {}

    virtual void Activate();
    virtual void Deactivate();

    void SetActive(bool bNewActive);
    inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
    
	inline void SetComponentTickEnabled(bool bEnabled)
    {
        bTickEnable = bEnabled;
        PrimaryComponentTick.SetTickEnabled(bEnabled);
    }

    inline bool IsComponentTickEnabled() const
    {
        return bTickEnable;
    }

    virtual void Serialize(FArchive& Ar) override;

    inline bool IsActive() { return bIsActive; }

    void SetOwner(AActor* Actor);
    virtual void OnOwnerChanged() {}
    AActor* GetOwner() const { return Owner; }
    UWorld* GetWorld() const;

    // 에디터에 노출할 프로퍼티 목록 반환. 하위 클래스에서 override하여 속성 추가.
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps);
    // 프로퍼티 값 변경 후 호출. 하위 클래스에서 override하여 부수효과(리소스 재로딩 등) 처리.
    virtual void PostEditProperty(const char* PropertyName);
    // 선택된 프록시의 소유 액터 컴포넌트가 디버그 시각화를 FScene에 기여
    // FPrimitiveProxy::CollectSelectedVisuals 에서 호출됨
    virtual void ContributeSelectedVisuals(FScene& Scene) const { (void)Scene; }

    FActorComponentTickFunction PrimaryComponentTick;

protected:
    // Component의 Tick은 UE 기준 Actor가 아닌 별도 시스템에서 돌아가나, 현재 관리를 위해 friend AActor로 설정. 추후 시스템이 완성되면 별도 매니저에서 관리하도록 리팩토링할 예정.
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction);

    AActor* Owner = nullptr;
    bool bTickEnable = true;

private:
    bool bIsActive = true;
    bool bAutoActivate = true;
};

