#pragma once
#include "Component/ActorComponent.h"
#include "ThirdParty/sol/sol.hpp"

class UScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UScriptComponent, UActorComponent)
	UScriptComponent() = default;
	~UScriptComponent() override = default;

	void PostDuplicate(UObject* Original) override;
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void SetScript(const FString& InScriptPath);
	const FString& GetScriptPath() const { return ScriptName; }

	// 컴포넌트가 언레지스터(삭제 직전)될 때 호출되어 스크립트 매니저에 알림
	virtual void OnUnregister() override;

private:
	FString ScriptName;
	sol::environment ScriptEnv;
	sol::table ScriptTable;
};
