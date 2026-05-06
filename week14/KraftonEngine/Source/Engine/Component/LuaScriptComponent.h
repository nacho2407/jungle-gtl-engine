#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Math/Vector.h"
#include <sol/sol.hpp>

class UPrimitiveComponent;
struct FHitResult;

class ULuaScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(ULuaScriptComponent, UActorComponent)

	ULuaScriptComponent();
	~ULuaScriptComponent();

	void InitializeLua();
	void ReloadScript();

	virtual void BeginPlay() override;
	virtual void EndPlay() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void Serialize(FArchive& Ar) override;

	const FString& GetScriptFile() const { return ScriptFile; }
	void SetScriptFile(const FString& InScriptFile) { ScriptFile = InScriptFile; }
	void DispatchOverlap(class AActor* OtherActor);

	// Lua script 의 환경(env)에서 인자 없는 전역 함수 하나를 호출. 함수가 없거나
	// nil 이면 조용히 false 반환 — 호출자는 lua 쪽 함수 정의 여부에 신경 쓸 필요 없음.
	bool CallFunction(const FString& FunctionName);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void EnsureDefaultScriptFile();
	void BindOwnerCollisionEvents();
	void ClearCollisionBindings();
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& HitResult);
	void HandleEndHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp);

	FString ScriptFile;
	
	sol::environment Env;
	sol::protected_function LuaBeginPlay;
	sol::protected_function LuaTick;
	sol::protected_function LuaEndPlay;
	sol::protected_function LuaOnOverlap;
	sol::protected_function LuaOnEndOverlap;
	sol::protected_function LuaOnHit;
	sol::protected_function LuaOnEndHit;
	TArray<UPrimitiveComponent*> BoundOverlapComponents;
	TArray<UPrimitiveComponent*> BoundHitComponents;
	TArray<FDelegateHandle> BeginOverlapHandles;
	TArray<FDelegateHandle> EndOverlapHandles;
	TArray<FDelegateHandle> HitHandles;
	TArray<FDelegateHandle> EndHitHandles;
};
