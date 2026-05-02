#include "LuaRuntime.h"

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#include "Core/CoroutineScheduler/LuaCoroutineScheduler.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Sound/SoundManager.h"
#include "LuaScript/LuaGameObjectProxy.h"
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

static int Cpp_Wait(lua_State* L)
{
    if (!lua_isnumber(L, 1))
    {
        return luaL_error(L, "wait() argument must be a number!");
    }

    float WaitTime = sol::stack::get<float>(L, 1);

    lua_pushthread(L);
    sol::thread CurrentThread(L, -1);
    lua_pop(L, 1);

    FLuaCoroutineScheduler::Get().AddTimeTask(CurrentThread, WaitTime);

    return lua_yield(L, 0);
}

static int Cpp_MoveTo(lua_State* L)
{
    FLuaGameObjectProxy* Proxy = sol::stack::get<FLuaGameObjectProxy*>(L, 1);
    float TargetX = (float)lua_tonumber(L, 2);
    float TargetY = (float)lua_tonumber(L, 3);

    if (!Proxy || !Proxy->IsValid())
    {
        return luaL_error(L, "move_to() requires a valid GameObject!");
    }

    FVector NewLoc = Proxy->GetLocation();
    NewLoc.X = TargetX;
    NewLoc.Y = TargetY;
    Proxy->SetLocation(NewLoc);

    lua_pushthread(L);
    sol::thread CurrentThread(L, -1);
    lua_pop(L, 1);

    auto Condition = [Proxy]() -> bool {
        if (!Proxy || !Proxy->IsValid()) return true;
        return Proxy->Velocity.LengthSquared() <= 0.01f;
        };

    FLuaCoroutineScheduler::Get().AddConditionTask(CurrentThread, Condition);

    return lua_yield(L, 0);
}

static int Cpp_WaitUntilMoveDone(lua_State* L)
{
    sol::optional<FLuaGameObjectProxy*> OptProxy = sol::stack::get<sol::optional<FLuaGameObjectProxy*>>(L, 1);
    if (!OptProxy)
    {
        return luaL_error(L, "wait_until_move_done() requires a valid GameObject as argument!");
    }

    FLuaGameObjectProxy* Proxy = OptProxy.value();

    lua_pushthread(L);
    sol::thread CurrentThread(L, -1);
    lua_pop(L, 1);

    auto Condition = [Proxy]() -> bool {
        if (!Proxy || !Proxy->IsValid()) return true;
        return Proxy->Velocity.LengthSquared() <= 0.01f;
        };

    FLuaCoroutineScheduler::Get().AddConditionTask(CurrentThread, Condition);

    return lua_yield(L, 0);
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
		"AddWorldOffset", &FLuaGameObjectProxy::AddWorldOffset,
		"PrintLocation", &FLuaGameObjectProxy::PrintLocation
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
        });

    Lua->set_function("wait", &Cpp_Wait);
    Lua->set_function("wait_until_move_done", &Cpp_WaitUntilMoveDone);
    Lua->set_function("move_to", &Cpp_MoveTo);
}
