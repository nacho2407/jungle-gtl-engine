#include "Runtime/Script/ScriptManager.h"

#include "Core/Logging/Log.h"
#include "Geometry/Transform.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "Runtime/Script/ScriptUtils.h"
#include "ThirdParty/sol/sol.hpp"

namespace
{
    FString LuaGetObjectName(UObject& Object)
    {
        return Object.GetName();
    }

    FString LuaGetObjectType(UObject& Object)
    {
        const FTypeInfo* TypeInfo = Object.GetTypeInfo();
        return TypeInfo ? TypeInfo->name : "";
    }

    bool LuaObjectIsA(UObject& Object, const FString& TypeName)
    {
        if (TypeName.empty())
        {
            return false;
        }

        for (const FTypeInfo* TypeInfo = Object.GetTypeInfo(); TypeInfo; TypeInfo = TypeInfo->Parent)
        {
            const FString NativeName = TypeInfo->name;
            const FString LuaStyleName = (NativeName.size() > 1 && (NativeName[0] == 'A' || NativeName[0] == 'U'))
                ? NativeName.substr(1)
                : NativeName;

            if (TypeName == NativeName || TypeName == LuaStyleName)
            {
                return true;
            }
        }
        return false;
    }
}

void FScriptManager::BindMathTypes()
{
    GLuaState->set_function("Log", [](const std::string& Msg)
    {
        UE_LOG(("[Lua] " + Msg + "\n").c_str());
    });

    GLuaState->set_function("LogWarning", [](const std::string& Msg)
    {
        UE_LOG(("[Lua Warning] " + Msg + "\n").c_str());
    });

    GLuaState->set_function("LogError", [](const std::string& Msg)
    {
        UE_LOG(("[Lua Error] " + Msg + "\n").c_str());
    });

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FName, "FName", []()
                           { return FName(); }, [](const char* Str)
                           { return FName(Str); }, [](const std::string& Str)
                           { return FName(Str.c_str()); })
    LUA_METHOD(ToString, ToString);
    LUA_META(to_string, [](const FName& Name)
             { return Name.ToString(); });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FVector, "Vector", []()
                           { return FVector(); }, [](float X, float Y, float Z)
                           { return FVector(X, Y, Z); })
    LUA_FIELD(X, X);
    LUA_FIELD(Y, Y);
    LUA_FIELD(Z, Z);
    LUA_FIELD(x, X);
    LUA_FIELD(y, Y);
    LUA_FIELD(z, Z);
    LUA_METHOD(Size, Size);
    LUA_OVERLOAD(Normalized, [](const FVector& Self)
                 { return Self.Normalized(); }, [](const FVector& Self, float Tolerance)
                 { return Self.Normalized(Tolerance); });
    LUA_META(equal_to, [](const FVector& A, const FVector& B)
             { return A == B; });
    LUA_META(addition, [](const FVector& A, const FVector& B)
             { return A + B; });
    LUA_META(subtraction, [](const FVector& A, const FVector& B)
             { return A - B; });
    LUA_META(unary_minus, [](const FVector& V)
             { return -V; });
    LUA_META(multiplication, sol::overload(
                                 [](const FVector& V, float S)
                                 {
                                     return V * S;
                                 },
                                 [](float S, const FVector& V)
                                 {
                                     return V * S;
                                 }));
    LUA_META(division, [](const FVector& V, float S)
             { return V / S; });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FQuat, "Quat", []()
                           { return FQuat(); }, [](float X, float Y, float Z, float W)
                           { return FQuat(X, Y, Z, W); })
    LUA_FIELD(X, X);
    LUA_FIELD(Y, Y);
    LUA_FIELD(Z, Z);
    LUA_FIELD(W, W);
    LUA_FIELD(x, X);
    LUA_FIELD(y, Y);
    LUA_FIELD(z, Z);
    LUA_FIELD(w, W);
    LUA_META(equal_to, [](const FQuat& A, const FQuat& B)
             { return A == B; });
    LUA_META(addition, [](const FQuat& A, const FQuat& B)
             { return A + B; });
    LUA_META(subtraction, [](const FQuat& A, const FQuat& B)
             { return A - B; });
    LUA_META(unary_minus, [](const FQuat& Q)
             { return -Q; });
    LUA_META(multiplication, sol::overload(
                                 [](const FQuat& Q, float S)
                                 {
                                     return Q * S;
                                 },
                                 [](float S, const FQuat& Q)
                                 {
                                     return Q * S;
                                 },
                                 [](const FQuat& A, const FQuat& B)
                                 {
                                     return A * B;
                                 },
                                 [](const FQuat& Q, const FVector& V)
                                 {
                                     return Q * V;
                                 }));
    LUA_META(division, [](const FQuat& Q, float S)
             { return Q / S; });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FTransform, "Transform",
                           []()
                           {
                               return FTransform();
                           },
                           [](const FVector& Translation, const FQuat& Rotation, const FVector& Scale)
                           {
                               return FTransform(Rotation, Translation, Scale);
                           })
    LUA_PROPERTY(Location, &FTransform::GetLocation, &FTransform::SetLocation);
    LUA_PROPERTY(Translation, &FTransform::GetTranslation, &FTransform::SetTranslation);
    LUA_PROPERTY(Rotation,
                 &FTransform::GetRotation,
                 [](FTransform& Self, const FQuat& InRotation)
                 {
                     Self.SetRotation(InRotation);
                 });
    LUA_PROPERTY(Scale, &FTransform::GetScale3D, &FTransform::SetScale3D);
    LUA_END_TYPE();
}

void FScriptManager::BindObjectTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR(GLuaState, UObject, "Object")
    LUA_RO_PROPERTY(UUID, GetUUID);
    LUA_METHOD(GetUUID, GetUUID);
    LUA_SET(GetName, &LuaGetObjectName);
    LUA_SET(GetType, &LuaGetObjectType);
    LUA_SET(IsA, &LuaObjectIsA);
    LUA_END_TYPE();
}
