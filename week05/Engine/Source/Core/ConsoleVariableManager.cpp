#include "ConsoleVariableManager.h"
#include <cstdlib>
#include <algorithm>

// --- FConsoleVariable ---

FConsoleVariable::FConsoleVariable(const FString& InName, int32 InDefault, const FString& InHelp)
	: Name(InName), Help(InHelp), Type(EConsoleVariableType::Int), IntValue(InDefault), FloatValue(static_cast<float>(InDefault))
{
}

FConsoleVariable::FConsoleVariable(const FString& InName, float InDefault, const FString& InHelp)
	: Name(InName), Help(InHelp), Type(EConsoleVariableType::Float), IntValue(static_cast<int32>(InDefault)), FloatValue(InDefault)
{
}

FConsoleVariable::FConsoleVariable(const FString& InName, const FString& InDefault, const FString& InHelp)
	: Name(InName), Help(InHelp), Type(EConsoleVariableType::String), StringValue(InDefault)
{
}

void FConsoleVariable::Set(int32 InValue)
{
	IntValue = InValue;
	FloatValue = static_cast<float>(InValue);
	if (OnChanged) OnChanged(this);
}

void FConsoleVariable::Set(float InValue)
{
	FloatValue = InValue;
	IntValue = static_cast<int32>(InValue);
	if (OnChanged) OnChanged(this);
}

void FConsoleVariable::Set(const FString& InValue)
{
	StringValue = InValue;
	if (OnChanged) OnChanged(this);
}

void FConsoleVariable::SetFromString(const FString& InValue)
{
	switch (Type)
	{
	case EConsoleVariableType::Int:
		Set(std::atoi(InValue.c_str()));
		break;
	case EConsoleVariableType::Float:
		Set(static_cast<float>(std::atof(InValue.c_str())));
		break;
	case EConsoleVariableType::String:
		Set(InValue);
		break;
	}
}

// --- FConsoleVariableManager ---

FConsoleVariableManager& FConsoleVariableManager::Get()
{
	static FConsoleVariableManager Instance;
	return Instance;
}

FConsoleVariable* FConsoleVariableManager::Register(const FString& Name, int32 Default, const FString& Help)
{
	FConsoleVariable* Var = new FConsoleVariable(Name, Default, Help);
	Variables[ToLower(Name)] = Var;
	return Var;
}

FConsoleVariable* FConsoleVariableManager::Register(const FString& Name, float Default, const FString& Help)
{
	FConsoleVariable* Var = new FConsoleVariable(Name, Default, Help);
	Variables[ToLower(Name)] = Var;
	return Var;
}

FConsoleVariable* FConsoleVariableManager::Register(const FString& Name, const FString& Default, const FString& Help)
{
	FConsoleVariable* Var = new FConsoleVariable(Name, Default, Help);
	Variables[ToLower(Name)] = Var;
	return Var;
}

FConsoleVariable* FConsoleVariableManager::Find(const FString& Name)
{
	auto It = Variables.find(ToLower(Name));
	return It != Variables.end() ? It->second : nullptr;
}

FString FConsoleVariableManager::ToLower(const FString& Str)
{
	FString Result = Str;
	std::transform(Result.begin(), Result.end(), Result.begin(),
		[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
	return Result;
}

bool FConsoleVariableManager::Execute(const char* CommandLine, FString& OutResult)
{
	// Parse: "t.MaxFPS 60" -> Name="t.MaxFPS", Value="60"
	// Or:    "t.MaxFPS"    -> Name="t.MaxFPS" (query)
	FString Line(CommandLine);

	size_t SpacePos = Line.find(' ');
	FString Name = (SpacePos != FString::npos) ? Line.substr(0, SpacePos) : Line;

	// Check commands first
	FString LowerName = ToLower(Name);
	auto CmdIt = Commands.find(LowerName);
	if (CmdIt != Commands.end())
	{
		CmdIt->second.Command(OutResult);
		return true;
	}

	FConsoleVariable* Var = Find(Name);
	if (!Var)
		return false;

	if (SpacePos != FString::npos)
	{
		FString Value = Line.substr(SpacePos + 1);
		Var->SetFromString(Value);
		OutResult = Name + " = " + Value;
	}
	else
	{
		switch (Var->GetType())
		{
		case EConsoleVariableType::Int:
			OutResult = Name + " = " + std::to_string(Var->GetInt());
			break;
		case EConsoleVariableType::Float:
			OutResult = Name + " = " + std::to_string(Var->GetFloat());
			break;
		case EConsoleVariableType::String:
			OutResult = Name + " = " + Var->GetString();
			break;
		}
		if (!Var->GetHelp().empty())
			OutResult += "  (" + Var->GetHelp() + ")";
	}
	return true;
}

void FConsoleVariableManager::RegisterCommand(const FString& Name, FConsoleCommand InCommand, const FString& Help)
{
	FCommandEntry Entry;
	Entry.Command = std::move(InCommand);
	Entry.Help = Help;
	Commands[ToLower(Name)] = std::move(Entry);
}

void FConsoleVariableManager::GetAllNames(std::function<void(const FString&)> Callback) const
{
	for (auto& Pair : Variables)
	{
		Callback(Pair.first);
	}
	for (auto& Pair : Commands)
	{
		Callback(Pair.first);
	}
}