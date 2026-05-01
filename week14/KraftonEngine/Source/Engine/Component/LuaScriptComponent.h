#pragma once

#include "Component/ActorComponent.h"
#include <sol/sol.hpp>

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

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void EnsureDefaultScriptFile();

	FString ScriptFile;
	
	sol::environment Env;
	sol::protected_function LuaBeginPlay;
	sol::protected_function LuaTick;
	sol::protected_function LuaEndPlay;
};
