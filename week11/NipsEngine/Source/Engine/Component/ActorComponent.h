#pragma once

#include "Object/Object.h"
#include "Core/PropertyTypes.h"

class AActor;

class UActorComponent : public UObject
{
public:
	DECLARE_CLASS(UActorComponent, UObject)
	
	virtual void BeginPlay();
	virtual void EndPlay() {};

	virtual void Activate();
	virtual void Deactivate();

	void ExecuteTick(float DeltaTime);
	void SetActive(bool bNewActive);
	inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
	inline void SetComponentTickEnabled(bool bEnabled) { bCanEverTick = bEnabled; }

	inline bool IsActive() { return bIsActive; }
	inline bool IsAutoActivate() { return bAutoActivate; }
	inline bool IsComponentTickEnabled() const { return bCanEverTick; }

	void SetOwner(AActor* Actor) { Owner = Actor; }
	AActor* GetOwner() const { return Owner; }

	// 에디터에 노출할 프로퍼티 목록 반환. 하위 클래스에서 override하여 속성 추가.
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	// 프로퍼티 값 변경 후 호출. 하위 클래스에서 override하여 부수효과(리소스 재로딩 등) 처리.
	void PostEditProperty(const char* PropertyName) override {}

	virtual void Serialize(FArchive& Ar) override;

	// CopyPropertiesFrom 은 UObject 에 정의됩니다.
	// 컴포넌트-컴포넌트 간 소유 관계(Owner, Parent 등)는 Duplicate() 호출 측에서 별도 처리해야 합니다.

	void SetTransient(bool bInTransient) { bTransient = bInTransient; }
	bool IsTransient() const { return bTransient; }

	void SetEditorOnly(bool bInEditorOnly) { bIsEditorOnly = bInEditorOnly; }
	bool IsEditorOnly() const { return bIsEditorOnly; }

protected:
	virtual void TickComponent(float DeltaTime) {}

protected:
	AActor* Owner = nullptr;

private:
	bool bIsActive = true;
	bool bAutoActivate = true;
	bool bCanEverTick = true;
	bool bTransient = false; // 런타임에만 존재해야 하며, 저장되어서는 안 되는 객체에 붙입니다. (UUID 컴포넌트)
	bool bIsEditorOnly = false;
};




