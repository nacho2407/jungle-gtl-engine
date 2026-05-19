#include "ReflectionLuaUtils.h"

#include "Object/Object.h"
#include "Math/Vector.h"

namespace ReflectionLuaUtils
{
FResolvedProperty FindProperty(UObject* Object, const FString& PropertyName)
{
    if (!Object)
    {
        return {};
    }

    const FName TargetName(PropertyName);

    for (FClassInfo* Info = Object->GetStaticClass(); Info; Info = Info->ParentClass)
    {
        for (FProperty* Prop : Info->ReflectedProperties)
        {
            if (!Prop || Prop->Name != TargetName)
            {
                continue;
            }

            uint8* Base = reinterpret_cast<uint8*>(Object);

            FResolvedProperty Result;
            Result.Property = Prop;
            Result.ValuePtr = Base + Prop->Offset;
            Result.OwnerClass = Info;
            return Result;
        }
    }

    return {};
}

sol::object GetProperty(sol::this_state State, UObject* Object, const FString& PropertyName)
{
    sol::state_view Lua(State);

    FResolvedProperty Resolved = FindProperty(Object, PropertyName);
    if (!Resolved.IsValid())
    {
        return sol::nil;
    }

    return PropertyValueToLua(State, Resolved.Property, Resolved.ValuePtr);
}

bool SetProperty(UObject* Object, const FString& PropertyName, const sol::object& Value)
{
    FResolvedProperty Resolved = FindProperty(Object, PropertyName);
    if (!Resolved.IsValid())
    {
        return false;
    }

    const bool bSet = LuaToPropertyValue(Resolved.Property, Resolved.ValuePtr, Value);
    if (bSet)
    {
        Object->PostEditChangeProperty({ PropertyName.c_str(), EPropertyChangeType::ValueSet });
    }
    return bSet;
}

sol::object PropertyValueToLua(sol::this_state State, FProperty* Property, void* ValuePtr)
{
    sol::state_view Lua(State);

    if (!Property || !ValuePtr)
    {
        return sol::nil;
    }

    if (dynamic_cast<FBoolProperty*>(Property))
    {
        return sol::make_object(Lua, *static_cast<bool*>(ValuePtr));
    }

    if (FNumericProperty* Numeric = dynamic_cast<FNumericProperty*>(Property))
    {
        if (Numeric->IsFloatingPoint())
        {
            if (Property->CPPType == "double")
            {
                return sol::make_object(Lua, *static_cast<double*>(ValuePtr));
            }

            return sol::make_object(Lua, *static_cast<float*>(ValuePtr));
        }

        if (Property->CPPType == "int8")
        {
            return sol::make_object(Lua, *static_cast<int8*>(ValuePtr));
        }
        if (Property->CPPType == "int16")
        {
            return sol::make_object(Lua, *static_cast<int16*>(ValuePtr));
        }
        if (Property->CPPType == "int32" || Property->CPPType == "int")
        {
            return sol::make_object(Lua, *static_cast<int32*>(ValuePtr));
        }
        if (Property->CPPType == "int64")
        {
            return sol::make_object(Lua, *static_cast<int64*>(ValuePtr));
        }
        if (Property->CPPType == "uint8" || Property->CPPType == "byte")
        {
            return sol::make_object(Lua, *static_cast<uint8*>(ValuePtr));
        }
        if (Property->CPPType == "uint16")
        {
            return sol::make_object(Lua, *static_cast<uint16*>(ValuePtr));
        }
        if (Property->CPPType == "uint32")
        {
            return sol::make_object(Lua, *static_cast<uint32*>(ValuePtr));
        }
        if (Property->CPPType == "uint64")
        {
            return sol::make_object(Lua, *static_cast<uint64*>(ValuePtr));
        }

    }

    if (dynamic_cast<FStrProperty*>(Property))
    {
        return sol::make_object(Lua, *static_cast<FString*>(ValuePtr));
    }

    if (dynamic_cast<FNameProperty*>(Property))
    {
        return sol::make_object(Lua, *static_cast<FName*>(ValuePtr));
    }

    if (FStructProperty* StructProp = dynamic_cast<FStructProperty*>(Property))
    {
        if (StructProp->CPPType == "FVector")
        {
            return sol::make_object(Lua, *static_cast<FVector*>(ValuePtr));
        }

        if (StructProp->CPPType == "FColor")
        {
            return sol::make_object(Lua, *static_cast<FColor*>(ValuePtr));
        }

        if (StructProp->CPPType == "FVector4")
        {
            return sol::make_object(Lua, *static_cast<FVector4*>(ValuePtr));
        }

        if (StructProp->CPPType == "FTransform")
        {
            return sol::make_object(Lua, *static_cast<FTransform*>(ValuePtr));
        }

        return sol::nil;
    }

    if (dynamic_cast<FObjectProperty*>(Property))
    {
        UObject* ObjectValue = *static_cast<UObject**>(ValuePtr);
        return sol::make_object(Lua, ObjectValue);
    }

    if (dynamic_cast<FEnumProperty*>(Property))
    {
        // 첫 버전은 숫자로 반환
        if (Property->ElementSize == sizeof(uint8))
        {
            return sol::make_object(Lua, static_cast<int32>(*static_cast<uint8*>(ValuePtr)));
        }
        if (Property->ElementSize == sizeof(int32))
        {
            return sol::make_object(Lua, *static_cast<int32*>(ValuePtr));
        }

        return sol::nil;
    }

    return sol::nil;
}

bool LuaToPropertyValue(FProperty* Property, void* ValuePtr, const sol::object& Value)
{
    if (!Property || !ValuePtr || !Value.valid() || Value == sol::nil)
    {
        return false;
    }

    if (dynamic_cast<FBoolProperty*>(Property))
    {
        if (!Value.is<bool>())
        {
            return false;
        }

        *static_cast<bool*>(ValuePtr) = Value.as<bool>();
        return true;
    }

    if (FNumericProperty* Numeric = dynamic_cast<FNumericProperty*>(Property))
    {
        if (!Value.is<double>() && !Value.is<int>())
        {
            return false;
        }

        if (Numeric->IsFloatingPoint())
        {
            if (Property->CPPType == "double")
            {
                *static_cast<double*>(ValuePtr) = Value.as<double>();
            }
            else
            {
                *static_cast<float*>(ValuePtr) = static_cast<float>(Value.as<double>());
            }
            return true;
        }

        if (Property->CPPType == "int8")
        {
            *static_cast<int8*>(ValuePtr) = static_cast<int8>(Value.as<int>());
            return true;
        }
        if (Property->CPPType == "int16")
        {
            *static_cast<int16*>(ValuePtr) = static_cast<int16>(Value.as<int>());
            return true;
        }
        if (Property->CPPType == "int32" || Property->CPPType == "int")
        {
            *static_cast<int32*>(ValuePtr) = static_cast<int32>(Value.as<int>());
            return true;
        }
        if (Property->CPPType == "int64")
        {
            *static_cast<int64*>(ValuePtr) = static_cast<int64>(Value.as<int64>());
            return true;
        }
        if (Property->CPPType == "uint8" || Property->CPPType == "byte")
        {
            *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(Value.as<int>());
            return true;
        }
        if (Property->CPPType == "uint16")
        {
            *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(Value.as<int>());
            return true;
        }
        if (Property->CPPType == "uint32")
        {
            *static_cast<uint32*>(ValuePtr) = static_cast<uint32>(Value.as<uint32>());
            return true;
        }
        if (Property->CPPType == "uint64")
        {
            *static_cast<uint64*>(ValuePtr) = static_cast<uint64>(Value.as<uint64>());
            return true;
        }

    }

    if (dynamic_cast<FStrProperty*>(Property))
    {
        if (!Value.is<FString>())
        {
            return false;
        }

        *static_cast<FString*>(ValuePtr) = Value.as<FString>();
        return true;
    }

    if (dynamic_cast<FNameProperty*>(Property))
    {
        if (Value.is<FName>())
        {
            *static_cast<FName*>(ValuePtr) = Value.as<FName>();
            return true;
        }

        if (Value.is<FString>())
        {
            *static_cast<FName*>(ValuePtr) = FName(Value.as<FString>());
            return true;
        }

        return false;
    }

    if (FStructProperty* StructProp = dynamic_cast<FStructProperty*>(Property))
    {
        if (StructProp->CPPType == "FVector")
        {
            if (!Value.is<FVector>())
                return false;
            *static_cast<FVector*>(ValuePtr) = Value.as<FVector>();
            return true;
        }

        if (StructProp->CPPType == "FColor")
        {
            if (!Value.is<FColor>())
                return false;
            *static_cast<FColor*>(ValuePtr) = Value.as<FColor>();
            return true;
        }

        if (StructProp->CPPType == "FVector4")
        {
            if (!Value.is<FVector4>())
                return false;
            *static_cast<FVector4*>(ValuePtr) = Value.as<FVector4>();
            return true;
        }

        if (StructProp->CPPType == "FTransform")
        {
            if (!Value.is<FTransform>())
                return false;
            *static_cast<FTransform*>(ValuePtr) = Value.as<FTransform>();
            return true;
        }
        if (StructProp->CPPType == "FVector2")
        {
            if (!Value.is<FVector2>())
                return false;
            *static_cast<FVector2*>(ValuePtr) = Value.as<FVector2>();
            return true;
        }
        return false;
    }

    if (dynamic_cast<FObjectProperty*>(Property))
    {
        if (!Value.is<UObject*>())
        {
            return false;
        }

        *static_cast<UObject**>(ValuePtr) = Value.as<UObject*>();
        return true;
    }

    if (dynamic_cast<FEnumProperty*>(Property))
    {
        if (!Value.is<int>())
        {
            return false;
        }

        const int EnumValue = Value.as<int>();

        if (Property->ElementSize == sizeof(uint8))
        {
            *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(EnumValue);
            return true;
        }

        if (Property->ElementSize == sizeof(int32))
        {
            *static_cast<int32*>(ValuePtr) = static_cast<int32>(EnumValue);
            return true;
        }

        return false;
    }

    return false;
}
} // namespace ReflectionLuaUtils