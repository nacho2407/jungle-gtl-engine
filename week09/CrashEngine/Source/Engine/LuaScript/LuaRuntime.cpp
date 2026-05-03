#include "LuaRuntime.h"

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#include "Core/CoroutineScheduler/LuaCoroutineScheduler.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Sound/SoundManager.h"
#include "LuaScript/LuaActionLibrary.h"
#include "LuaScript/LuaComponentProxy.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "LuaScript/LuaInputProxy.h"
#include "LuaScript/LuaPersistentStorage.h"
#include "Math/Vector.h"

namespace
{
FString LuaObjectToString(const sol::object& Object)
{
    switch (Object.get_type())
    {
    case sol::type::nil:
        return "nil";

    case sol::type::boolean:
        return Object.as<bool>() ? "true" : "false";

    case sol::type::number:
    {
        std::ostringstream Stream;
        Stream << Object.as<double>();
        return Stream.str();
    }

    case sol::type::string:
        return Object.as<std::string>();

    case sol::type::userdata:
    {
        if (Object.is<FVector>())
        {
            const FVector V = Object.as<FVector>();
            char Buffer[128];
            std::snprintf(Buffer, sizeof(Buffer), "Vector(%.3f, %.3f, %.3f)", V.X, V.Y, V.Z);
            return Buffer;
        }

        return "userdata";
    }

    case sol::type::function:
        return "function";

    case sol::type::table:
        return "table";

    case sol::type::thread:
        return "thread";

    default:
        return "unknown";
    }
}
} // namespace

void FLuaRuntime::Initialize()
{
    if (bInitialized)
    {
        return;
    }

    Lua = std::make_unique<sol::state>();

    OpenLibraries();

    const int JitModeResult = luaJIT_setmode(
        Lua->lua_state(),
        0,
        LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON
	);

    if (JitModeResult == 0)
    {
        UE_LOG(Lua, Warning, "luaJIT_setmode failed. Lua state was created, but JIT mode was not explicitly enabled.");
    }

    BindUtilityFunctions();
    BindEngineTypes();

    bInitialized = true;

#ifdef LUAJIT_VERSION
    UE_LOG(Lua, Info, "LuaJIT runtime initialized: %s", LUAJIT_VERSION);
#else
    UE_LOG(Lua, Info, "Lua runtime initialized.");
#endif
}

void FLuaRuntime::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    if (Lua)
    {
        Lua->collect_garbage();
        Lua.reset();
    }

    bInitialized = false;

    UE_LOG(Lua, Info, "Lua runtime shut down.");
}

sol::state& FLuaRuntime::GetState()
{
    if (!bInitialized)
    {
        Initialize();
    }

    return *Lua;
}

void FLuaRuntime::OpenLibraries()
{
    Lua->open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string,
        sol::lib::coroutine,
        sol::lib::jit
	);
}

void FLuaRuntime::BindUtilityFunctions()
{
    Lua->set_function("print", [](sol::variadic_args Args) {
			FString Message;
			bool bFirst = true;

			for (sol::object Arg : Args)
			{
				if (!bFirst)
				{
					Message += "\t";
				}

				Message += LuaObjectToString(Arg);
				bFirst = false;
			}

			UE_LOG(Lua, Info, "%s", Message.c_str());
		}
	);
}

void FLuaRuntime::BindEngineTypes()
{
    Lua->new_usertype<FVector>("Vector", sol::constructors<FVector(), FVector(float), FVector(float, float, float)>(),
		"X", &FVector::X, "Y", &FVector::Y, "Z", &FVector::Z,
		"Length", &FVector::Length, "LengthSquared", &FVector::LengthSquared, "Normalized", &FVector::Normalized,

		sol::meta_function::addition, [](const FVector& A, const FVector& B)
		{ return A + B; },

		sol::meta_function::subtraction, [](const FVector& A, const FVector& B)
		{ return A - B; },

		sol::meta_function::multiplication, sol::overload([](const FVector& V, float S) { return V * S; }, [](float S, const FVector& V) { return V * S; }),

		sol::meta_function::division, [](const FVector& V, float S) { return V / S; },

		sol::meta_function::to_string, [](const FVector& V) {
			std::ostringstream Stream;
			Stream << "Vector(" << std::fixed << std::setprecision(3) << V.X << ", " << V.Y << ", " << V.Z << ")";
			return Stream.str();
		}
	);

    Lua->new_usertype<FLuaGameObjectProxy>("GameObject", sol::no_constructor,
		"UUID", sol::property(&FLuaGameObjectProxy::GetUUID),
		"Location", sol::property(&FLuaGameObjectProxy::GetLocation, &FLuaGameObjectProxy::SetLocation),
		"Velocity", &FLuaGameObjectProxy::Velocity,
		"IsValid", &FLuaGameObjectProxy::IsValid,
        "GetForwardVector", &FLuaGameObjectProxy::GetForwardVector,
        "GetRightVector", &FLuaGameObjectProxy::GetRightVector,
        "GetUpVector", &FLuaGameObjectProxy::GetUpVector,
		"AddWorldOffset", &FLuaGameObjectProxy::AddWorldOffset,
		"ApplyDamage", &FLuaGameObjectProxy::ApplyDamage,
		"HasTag", &FLuaGameObjectProxy::HasTag,
		"AddTag", &FLuaGameObjectProxy::AddTag,
		"RemoveTag", &FLuaGameObjectProxy::RemoveTag,
		"PrintLocation", &FLuaGameObjectProxy::PrintLocation,
        "AddComponent", &FLuaGameObjectProxy::AddComponent,
        "GetComponent", &FLuaGameObjectProxy::GetComponent
	);

    // SoundManager 등록
    Lua->new_usertype<FSoundManager>("SoundManager", sol::no_constructor,
        "LoadSound", &FSoundManager::LoadSound,
        "LoadSoundsFromDirectory", &FSoundManager::LoadSoundsFromDirectory,
        "PlayBGM", &FSoundManager::PlayBGM,
        "StopBGM", &FSoundManager::StopBGM,
        "PlaySFX", &FSoundManager::PlaySFX,
        "StopAllSFX", &FSoundManager::StopAllSFX,
        "SetMasterVolume", &FSoundManager::SetMasterVolume,
        "SetBGMVolume", &FSoundManager::SetBGMVolume,
        "SetSFXVolume", &FSoundManager::SetSFXVolume
    );

    Lua->set_function("GetSoundManager", []() -> FSoundManager& {
			return FSoundManager::Get();
		}
	);

	Lua->new_usertype<FLuaInputProxy>("InputProxy", sol::no_constructor,
		"IsKeyDown", &FLuaInputProxy::IsKeyDown,
		"WasKeyPressed", &FLuaInputProxy::WasKeyPressed,
		"WasKeyReleased", &FLuaInputProxy::WasKeyReleased,

		"IsMouseButtonDown", &FLuaInputProxy::IsMouseButtonDown,
		"WasMouseButtonPressed", &FLuaInputProxy::WasMouseButtonPressed,
		"WasMouseButtonReleased", &FLuaInputProxy::WasMouseButtonReleased,

		"IsGamepadConnected", &FLuaInputProxy::IsGamepadConnected,
		"IsGamepadButtonDown", &FLuaInputProxy::IsGamepadButtonDown,
		"WasGamepadButtonPressed", &FLuaInputProxy::WasGamepadButtonPressed,
		"WasGamepadButtonReleased", &FLuaInputProxy::WasGamepadButtonReleased,

		"GetAxis", &FLuaInputProxy::GetAxis
	);

    static FLuaInputProxy GLuaInputProxy;

	// sol2를 이용하여 Lua 전역 테이블에 Input 추가
    (*Lua)["Input"] = &GLuaInputProxy;

    // Coroutine 관련 함수 바인딩
    LuaActionLibrary::Bind(*Lua);
    FLuaPersistentStorage::Bind(*Lua);

	Lua->new_usertype<FLuaComponentProxy>("Component", sol::no_constructor,
		"UUID", sol::property(&FLuaComponentProxy::GetUUID),
		"IsValid", &FLuaComponentProxy::IsValid,
		"GetClassName", &FLuaComponentProxy::GetComponentClassName,

        "GetForwardVector", &FLuaComponentProxy::GetForwardVector,
        "GetRightVector", &FLuaComponentProxy::GetRightVector,
        "GetUpVector", &FLuaComponentProxy::GetUpVector,

		"GetRelativeLocation", &FLuaComponentProxy::GetRelativeLocation,
		"SetRelativeLocation", &FLuaComponentProxy::SetRelativeLocation,
		"GetRelativeScale", &FLuaComponentProxy::GetRelativeScale,
		"SetRelativeScale", &FLuaComponentProxy::SetRelativeScale,
		"GetRelativeRotation", &FLuaComponentProxy::GetRelativeRotation,
		"SetRelativeRotation", &FLuaComponentProxy::SetRelativeRotation,

		"SetVisible", &FLuaComponentProxy::SetVisible,
		"SetVisibleInGame", &FLuaComponentProxy::SetVisibleInGame,
		"SetVisibleInEditor", &FLuaComponentProxy::SetVisibleInEditor,

		"SetStaticMesh", &FLuaComponentProxy::SetStaticMesh,
		"GetStaticMeshPath", &FLuaComponentProxy::GetStaticMeshPath,
		"GetNumMaterials", &FLuaComponentProxy::GetNumMaterials,
		"SetMaterial", &FLuaComponentProxy::SetMaterial,

		"SetText", &FLuaComponentProxy::SetText,
		"GetText", &FLuaComponentProxy::GetText,
		"SetFontSize", &FLuaComponentProxy::SetFontSize,
		"SetBillboard", &FLuaComponentProxy::SetBillboard,

		"SetIntensity", &FLuaComponentProxy::SetIntensity,
		"SetLightColor", &FLuaComponentProxy::SetLightColor,
		"SetAffectsWorld", &FLuaComponentProxy::SetAffectsWorld,
		"SetCastShadows", &FLuaComponentProxy::SetCastShadows,

		"SetSphereRadius", &FLuaComponentProxy::SetSphereRadius,
        "SetBoxExtent", &FLuaComponentProxy::SetBoxExtent,
		"SetCapsuleSize", &FLuaComponentProxy::SetCapsuleSize,
		"SetCapsuleRadius", &FLuaComponentProxy::SetCapsuleRadius,
		"SetCapsuleHalfHeight", &FLuaComponentProxy::SetCapsuleHalfHeight,

        "CallFunction", &FLuaComponentProxy::CallFunction
	);
}
