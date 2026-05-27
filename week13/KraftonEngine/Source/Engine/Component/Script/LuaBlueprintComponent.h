#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Source/Engine/Component/Script/LuaBlueprintComponent.generated.h"
#include <sol/sol.hpp>

class ULuaBlueprintAsset;
class UPrimitiveComponent;
struct FHitResult;

UCLASS()
class ULuaBlueprintComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	ULuaBlueprintComponent();
	~ULuaBlueprintComponent() override;

	UFUNCTION(Callable, Exec, Category="Lua Blueprint")
	bool ReloadBlueprint();

	UFUNCTION(Callable, Exec, Category="Lua Blueprint")
	bool CallFunction(const FString& FunctionName);

	void SetBlueprintPath(const FString& InPath) { BlueprintPath = InPath; }
	const FString& GetBlueprintPath() const { return BlueprintPath; }
	ULuaBlueprintAsset* GetBlueprintAsset() const { return BlueprintAsset; }

	void BeginPlay() override;
	void EndPlay() override;
	void BeginDestroy() override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	void PreGetEditableProperties() override;
	void PostEditProperty(const char* PropertyName) override;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	bool LoadBlueprintAsset();
	bool InitializeLua();
	void ClearLuaRuntime();
	void BindOwnerCollisionEvents();
	void ClearCollisionBindings();
	FString GetRuntimeName() const;

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

private:
	UPROPERTY(Edit, Save, Category="Lua Blueprint", DisplayName="Blueprint", AssetType="ULuaBlueprintAsset")
	FString BlueprintPath;

	ULuaBlueprintAsset* BlueprintAsset = nullptr;
	uint32              LoadedBlueprintVersion = 0;

	sol::environment Env;
	sol::protected_function LuaBeginPlay;
	sol::protected_function LuaTick;
	sol::protected_function LuaEndPlay;
	sol::protected_function LuaOnOverlap;
	sol::protected_function LuaOnEndOverlap;
	sol::protected_function LuaOnHit;
	sol::protected_function LuaOnEndHit;

	// Lua 콜백 진입 카운터. obj:Destroy() 처럼 Lua 안에서 자기 자신을 destroy 하면
	// EndPlay → ClearLuaRuntime 이 mid-execution 으로 호출되어 sol::env / function 이 nil 화 →
	// 복귀 시 lua51 SIGSEGV. 재진입 중에는 정리를 미루고 outer 진입에서 처리한다.
	int32 LuaCallDepth = 0;
	bool  bPendingLuaCleanup = false;

	struct FLuaCallScope
	{
		ULuaBlueprintComponent* Owner;
		explicit FLuaCallScope(ULuaBlueprintComponent* InOwner) : Owner(InOwner) { ++Owner->LuaCallDepth; }
		~FLuaCallScope() { --Owner->LuaCallDepth; }
	};

	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundOverlapComponents;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundHitComponents;
	TArray<FDelegateHandle> BeginOverlapHandles;
	TArray<FDelegateHandle> EndOverlapHandles;
	TArray<FDelegateHandle> HitHandles;
	TArray<FDelegateHandle> EndHitHandles;
};
