#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
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

	virtual void BeginPlay() override;
	virtual void EndPlay() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void Serialize(FArchive& Ar) override;

	const FString& GetScriptFile() const { return ScriptFile; }
	void SetScriptFile(const FString& InScriptFile) { ScriptFile = InScriptFile; }
	void DispatchOverlap(class AActor* OtherActor);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void EnsureDefaultScriptFile();
	void BindOwnerOverlapEvents();
	void ClearOverlapBindings();
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

	FString ScriptFile;
	
	sol::environment Env;
	sol::protected_function LuaBeginPlay;
	sol::protected_function LuaTick;
	sol::protected_function LuaEndPlay;
	sol::protected_function LuaOnOverlap;
	sol::protected_function LuaOnEndOverlap;
	TArray<UPrimitiveComponent*> BoundOverlapComponents;
	TArray<FDelegateHandle> BeginOverlapHandles;
	TArray<FDelegateHandle> EndOverlapHandles;
};
