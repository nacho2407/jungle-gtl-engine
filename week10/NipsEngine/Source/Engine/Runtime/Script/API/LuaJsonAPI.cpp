#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    bool IsArrayTable(const sol::table& Table, int32& OutMaxIndex)
    {
        int32 Count = 0;
        int32 MaxIndex = 0;

        for (const auto& Pair : Table)
        {
            const sol::object Key = Pair.first;
            if (!Key.is<int32>())
            {
                return false;
            }

            const int32 Index = Key.as<int32>();
            if (Index < 1)
            {
                return false;
            }

            ++Count;
            MaxIndex = std::max(MaxIndex, Index);
        }

        OutMaxIndex = MaxIndex;
        return Count == MaxIndex;
    }

    json::JSON LuaObjectToJson(const sol::object& Object, int32 Depth = 0)
    {
        if (Depth > 64 || !Object.valid() || Object == sol::nil)
        {
            return json::JSON();
        }

        switch (Object.get_type())
        {
        case sol::type::boolean:
            return json::JSON(Object.as<bool>());
        case sol::type::number:
        {
            const double Value = Object.as<double>();
            if (std::isfinite(Value) && std::floor(Value) == Value)
            {
                return json::JSON(static_cast<long>(Value));
            }
            return json::JSON(Value);
        }
        case sol::type::string:
            return json::JSON(Object.as<FString>());
        case sol::type::table:
        {
            const sol::table Table = Object.as<sol::table>();
            int32 MaxIndex = 0;
            if (IsArrayTable(Table, MaxIndex))
            {
                json::JSON Array = json::Array();
                for (int32 Index = 1; Index <= MaxIndex; ++Index)
                {
                    Array.append(LuaObjectToJson(Table.get<sol::object>(Index), Depth + 1));
                }
                return Array;
            }

            json::JSON JsonObject = json::Object();
            for (const auto& Pair : Table)
            {
                const sol::object Key = Pair.first;
                if (Key.is<FString>())
                {
                    JsonObject[Key.as<FString>()] = LuaObjectToJson(Pair.second, Depth + 1);
                }
            }
            return JsonObject;
        }
        default:
            return json::JSON();
        }
    }

    sol::object JsonToLuaObject(sol::state_view Lua, const json::JSON& Json)
    {
        switch (Json.JSONType())
        {
        case json::JSON::Class::Object:
        {
            sol::table Table = Lua.create_table();
            for (const auto& Pair : Json.ObjectRange())
            {
                Table[Pair.first] = JsonToLuaObject(Lua, Pair.second);
            }
            return sol::make_object(Lua, Table);
        }
        case json::JSON::Class::Array:
        {
            sol::table Table = Lua.create_table();
            int32 Index = 1;
            for (const json::JSON& Value : Json.ArrayRange())
            {
                Table[Index++] = JsonToLuaObject(Lua, Value);
            }
            return sol::make_object(Lua, Table);
        }
        case json::JSON::Class::String:
            return sol::make_object(Lua, Json.ToString());
        case json::JSON::Class::Floating:
            return sol::make_object(Lua, Json.ToFloat());
        case json::JSON::Class::Integral:
            return sol::make_object(Lua, static_cast<int32>(Json.ToInt()));
        case json::JSON::Class::Boolean:
            return sol::make_object(Lua, Json.ToBool());
        case json::JSON::Class::Null:
        default:
            return sol::make_object(Lua, sol::nil);
        }
    }
}

namespace FLuaEngineAPI
{
    void BindJson(sol::state& Lua, sol::table& API)
    {
        sol::table Json = Lua.create_table();

        Json["Encode"] = [](sol::object Value) -> FString
        {
            return LuaObjectToJson(Value).dump();
        };

        Json["Decode"] = [](sol::this_state State, const FString& Text) -> sol::object
        {
            if (Text.empty())
            {
                return sol::make_object(State, sol::nil);
            }

            sol::state_view Lua(State);
            json::JSON Data = json::JSON::Load(Text);
            return JsonToLuaObject(Lua, Data);
        };

        API["Json"] = Json;
    }
}
