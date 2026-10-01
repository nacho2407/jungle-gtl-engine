#pragma once

#include <memory>

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "LuaScript/LuaIncludes.h"

class FLuaRuntime : public TSingleton<FLuaRuntime>
{
    friend class TSingleton<FLuaRuntime>;

public:
    void Initialize();
    void Shutdown();

    bool IsInitialized() const { return bInitialized; }

    sol::state& GetState();

private:
    FLuaRuntime() = default;
    ~FLuaRuntime() = default;

    void OpenLibraries();

	// 유틸리티 함수를 추가할 때는 이 함수에 추가
    void BindUtilityFunctions();
    void BindEngineTypes();

private:
    std::unique_ptr<sol::state> Lua;
    bool bInitialized = false;
};