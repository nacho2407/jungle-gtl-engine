#include "LuaScriptManager.h"

#include "Core/Log.h"
#include "GameFramework/AActor.h"
#include "Platform/Paths.h"
#include "Math/Vector.h"
#include <filesystem>
#include <fstream>

std::unique_ptr<sol::state> FLuaScriptManager::Lua;

void FLuaScriptManager::Initialize()
{
	Lua = std::make_unique<sol::state>();
	Lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math);
	RegisterBindings(*Lua);
}

void FLuaScriptManager::Shutdown()
{
	Lua.reset();
}

FString FLuaScriptManager::ResolveScriptPath(const FString& ScriptFile)
{
	std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	return FPaths::ToUtf8(FullPath);
}

bool FLuaScriptManager::OpenOrCreateScript(const FString& ScriptFile)
{
	std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	if (!std::filesystem::exists(FullPath))
	{
		std::ofstream Out(FullPath);
		if (!Out)
		{
			return false;
		}
	}

	HINSTANCE HInst = ShellExecuteW(nullptr, L"open", FullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	if ((INT_PTR)HInst <= 32)
	{
		return false;
	}

	return true;
}

sol::state& FLuaScriptManager::GetState()
{
	return *Lua;
}

void FLuaScriptManager::RegisterBindings(sol::state& Lua)
{
	Lua.set_function("print", [](sol::variadic_args Args)
	{
		FString Message;

		for (auto Arg : Args)
		{
			if (!Message.empty())
			{
				Message += "\t";
			}

			Message += Arg.as<FString>();
		}

		UE_LOG("[Lua] %s", Message.c_str());
	});

	Lua.new_usertype<FVector>("Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,
		"Length", &FVector::Length,
		"Normalize", &FVector::Normalize,
		"Normalized", &FVector::Normalized,
		"Dot", &FVector::Dot,
		"Cross", sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::Cross),
		static_cast<FVector(*)(const FVector&, const FVector&)>(&FVector::Cross)
	),
		"Distance", &FVector::Distance,
		"DistSquared", &FVector::DistSquared,
		"Lerp", &FVector::Lerp,
		sol::meta_function::addition, sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator+),
		static_cast<FVector(FVector::*)(float) const>(&FVector::operator+)
	),
		sol::meta_function::subtraction, sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator-),
		static_cast<FVector(FVector::*)(float) const>(&FVector::operator-)
	),
		sol::meta_function::multiplication, &FVector::operator*,
		sol::meta_function::division, &FVector::operator/,
		"Zero", []() { return FVector::ZeroVector; },
		"One", []() { return FVector::OneVector; },
		"Up", []() { return FVector::UpVector; },
		"Down", []() { return FVector::DownVector; },
		"Forward", []() { return FVector::ForwardVector; },
		"Backward", []() { return FVector::BackwardVector; },
		"Right", []() { return FVector::RightVector; },
		"Left", []() { return FVector::LeftVector; },
		"XAxis", []() { return FVector::XAxisVector; },
		"YAxis", []() { return FVector::YAxisVector; },
		"ZAxis", []() { return FVector::ZAxisVector; });

	Lua.new_usertype<AActor>("Actor",
		"Location", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorLocation();
	},
		[](AActor& Actor, const FVector& Location)
	{
		Actor.SetActorLocation(Location);
	}
	),

		"Scale", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorScale();
	},
		[](AActor& Actor, const FVector& Scale)
	{
		Actor.SetActorScale(Scale);
	}
	),

		"UUID", sol::property([](AActor& Actor)
	{
		return Actor.GetUUID();
	}),

		"Name", sol::property([](AActor& Actor)
	{
		return Actor.GetFName().ToString();
	}),

		"PrintLocation", [](AActor& Actor)
	{
		FVector Location = Actor.GetActorLocation();
		UE_LOG("[Lua] Actor Location: %.2f %.2f %.2f", Location.X, Location.Y, Location.Z);
	}
	);
}
