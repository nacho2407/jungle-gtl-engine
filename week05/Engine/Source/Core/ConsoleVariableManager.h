#pragma once
#include "CoreMinimal.h"
#include <functional>

enum class EConsoleVariableType : uint8
{
	Int,
	Float,
	String,
};

class ENGINE_API FConsoleVariable
{
public:
	using FOnChanged = std::function<void(FConsoleVariable*)>;

	FConsoleVariable(const FString& InName, int32 InDefault, const FString& InHelp = "");
	FConsoleVariable(const FString& InName, float InDefault, const FString& InHelp = "");
	FConsoleVariable(const FString& InName, const FString& InDefault, const FString& InHelp = "");

	const FString& GetName() const { return Name; }
	const FString& GetHelp() const { return Help; }
	EConsoleVariableType GetType() const { return Type; }

	int32 GetInt() const { return IntValue; }
	float GetFloat() const { return FloatValue; }
	const FString& GetString() const { return StringValue; }

	void Set(int32 InValue);
	void Set(float InValue);
	void Set(const FString& InValue);
	void SetFromString(const FString& InValue);

	void SetOnChanged(FOnChanged InCallback) { OnChanged = std::move(InCallback); }

private:
	FString Name;
	FString Help;
	EConsoleVariableType Type;

	int32 IntValue = 0;
	float FloatValue = 0.0f;
	FString StringValue;

	FOnChanged OnChanged;
};

class ENGINE_API FConsoleVariableManager
{
public:
	static FConsoleVariableManager& Get();

	FConsoleVariable* Register(const FString& Name, int32 Default, const FString& Help = "");
	FConsoleVariable* Register(const FString& Name, float Default, const FString& Help = "");
	FConsoleVariable* Register(const FString& Name, const FString& Default, const FString& Help = "");

	FConsoleVariable* Find(const FString& Name);
	bool Execute(const char* CommandLine, FString& OutResult);
	void GetAllNames(std::function<void(const FString&)> Callback) const;

	using FConsoleCommand = std::function<void(FString&)>;
	void RegisterCommand(const FString& Name, FConsoleCommand InCommand, const FString& Help = "");

private:
	FConsoleVariableManager() = default;
	static FString ToLower(const FString& Str);
	TMap<FString, FConsoleVariable*> Variables;

	struct FCommandEntry
	{
		FConsoleCommand Command;
		FString Help;
	};
	TMap<FString, FCommandEntry> Commands;
};
