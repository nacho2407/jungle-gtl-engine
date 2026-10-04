#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Core/Random/EngineRandom.h"

namespace FLuaEngineAPI
{
    void BindRandom(sol::state& Lua, sol::table& API)
    {
        sol::table Random = Lua.create_table();

        Random["SetSeed"] = [](uint32 Seed)
        {
            FEngineRandom::Get().SetSeed(Seed);
        };

        Random["RandomFloat01"] = []() -> float
        {
            return FEngineRandom::Get().RandomFloat01();
        };

        Random["RandomFloat"] = [](float Min, float Max) -> float
        {
            return FEngineRandom::Get().RandomFloat(Min, Max);
        };

        Random["RandomInt"] = [](int32 Min, int32 Max) -> int32
        {
            return FEngineRandom::Get().RandomInt(Min, Max);
        };

        Random["RandomBool"] = [](float Probability) -> bool
        {
            return FEngineRandom::Get().RandomBool(Probability);
        };

        API["Random"] = Random;
    }
}
