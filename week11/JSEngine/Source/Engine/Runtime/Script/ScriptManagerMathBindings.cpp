#include "Runtime/Script/ScriptManager.h"

#include "Core/Logging/Log.h"
#include "Core/CollisionTypes.h"
#include "Geometry/Transform.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Object/Object.h"
#include "Runtime/Script/ScriptUtils.h"
#include "ReflectionSystem/ReflectionLuaUtils.h"
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
        UE_LOG_WARNING(("[Lua Warning] " + Msg + "\n").c_str());
    });

    GLuaState->set_function("LogError", [](const std::string& Msg)
    {
        UE_LOG_ERROR(("[Lua Error] " + Msg + "\n").c_str());
    });

    GLuaState->set_function("WaitForSeconds", [](sol::this_state State, float Seconds)
    {
        sol::state_view Lua(State);
        sol::table Table = Lua.create_table();
        Table["type"] = "seconds";
        Table["value"] = Seconds;
        return Table;
    });

    GLuaState->set_function("WaitForUnscaledSeconds", [](sol::this_state State, float Seconds)
    {
        sol::state_view Lua(State);
        sol::table Table = Lua.create_table();
        Table["type"] = "unscaled_seconds";
        Table["value"] = Seconds;
        return Table;
    });

    GLuaState->set_function("WaitForFrames", [](sol::this_state State, int Frames)
    {
        sol::state_view Lua(State);
        sol::table Table = Lua.create_table();
        Table["type"] = "frames";
        Table["value"] = Frames;
        return Table;
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
    LUA_METHOD(SizeSquared, SizeSquared);
    LUA_METHOD(Size2D, Size2D);
    LUA_METHOD(SizeSquared2D, SizeSquared2D);
    LUA_SET(Length, [](const FVector& Self)
            { return Self.Size(); });
    LUA_SET(LengthSquared, [](const FVector& Self)
            { return Self.SizeSquared(); });
    LUA_OVERLOAD(Normalize, [](FVector& Self)
                 { return Self.Normalize(); }, [](FVector& Self, float Tolerance)
                 { return Self.Normalize(Tolerance); });
    LUA_OVERLOAD(GetSafeNormal, [](const FVector& Self)
                 { return Self.GetSafeNormal(); }, [](const FVector& Self, float Tolerance)
                 { return Self.GetSafeNormal(Tolerance); });
    LUA_OVERLOAD(Normalized, [](const FVector& Self)
                 { return Self.Normalized(); }, [](const FVector& Self, float Tolerance)
                 { return Self.Normalized(Tolerance); });
    LUA_OVERLOAD(GetSafeNormal2D, [](const FVector& Self)
                 { return Self.GetSafeNormal2D(); }, [](const FVector& Self, float Tolerance)
                 { return Self.GetSafeNormal2D(Tolerance); });
    LUA_OVERLOAD(Normalized2D, [](const FVector& Self)
                 { return Self.GetSafeNormal2D(); }, [](const FVector& Self, float Tolerance)
                 { return Self.GetSafeNormal2D(Tolerance); });
    LUA_SET(Dot, [](const FVector& Self, const FVector& Other)
            { return Self.DotProduct(Other); });
    LUA_SET(DotProduct, [](const FVector& Self, const FVector& Other)
            { return Self.DotProduct(Other); });
    LUA_SET(Cross, [](const FVector& Self, const FVector& Other)
            { return Self.CrossProduct(Other); });
    LUA_SET(CrossProduct, [](const FVector& Self, const FVector& Other)
            { return Self.CrossProduct(Other); });
    LUA_SET(DistanceTo, [](const FVector& Self, const FVector& Other)
            { return FVector::Dist(Self, Other); });
    LUA_SET(DistanceSquaredTo, [](const FVector& Self, const FVector& Other)
            { return FVector::DistSquared(Self, Other); });
    LUA_SET(Distance, [](const FVector& A, const FVector& B)
            { return FVector::Dist(A, B); });
    LUA_SET(Dist, [](const FVector& A, const FVector& B)
            { return FVector::Dist(A, B); });
    LUA_SET(DistSquared, [](const FVector& A, const FVector& B)
            { return FVector::DistSquared(A, B); });
    LUA_SET(Lerp, [](const FVector& A, const FVector& B, float T)
            { return FVector::Lerp(A, B, T); });
    LUA_SET(Zero, []()
            { return FVector::ZeroVector; });
    LUA_SET(One, []()
            { return FVector::OneVector; });
    LUA_SET(Forward, []()
            { return FVector::ForwardVector; });
    LUA_SET(Backward, []()
            { return FVector::BackwardVector; });
    LUA_SET(Right, []()
            { return FVector::RightVector; });
    LUA_SET(Left, []()
            { return FVector::LeftVector; });
    LUA_SET(Up, []()
            { return FVector::UpVector; });
    LUA_SET(Down, []()
            { return FVector::DownVector; });
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
    LUA_BEGIN_TYPE_FACTORY(GLuaState, FVector2, "Vector2", []()
                           { return FVector2(); }, [](float X, float Y)
                           { return FVector2(X, Y); })
    LUA_FIELD(X, X);
    LUA_FIELD(Y, Y);
    LUA_FIELD(x, X);
    LUA_FIELD(y, Y);
    LUA_SET(Zero, []()
            { return FVector2::Zero(); });
    LUA_SET(One, []()
            { return FVector2::One(); });
    LUA_SET(UnitX, []()
            { return FVector2::UnitX(); });
    LUA_SET(UnitY, []()
            { return FVector2::UnitY(); });
    LUA_META(equal_to, [](const FVector2& A, const FVector2& B)
             { return A == B; });
    LUA_META(addition, [](const FVector2& A, const FVector2& B)
             { return A + B; });
    LUA_META(subtraction, [](const FVector2& A, const FVector2& B)
             { return A - B; });
    LUA_META(unary_minus, [](const FVector2& V)
             { return -V; });
    LUA_META(multiplication, sol::overload(
                                 [](const FVector2& V, float S)
                                 {
                                     return V * S;
                                 },
                                 [](float S, const FVector2& V)
                                 {
                                     return V * S;
                                 }));
    LUA_META(division, [](const FVector2& V, float S)
             { return V / S; });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FVector4, "Vector4", []()
                           { return FVector4(); }, [](float X, float Y, float Z, float W)
                           { return FVector4(X, Y, Z, W); })
    LUA_FIELD(X, X);
    LUA_FIELD(Y, Y);
    LUA_FIELD(Z, Z);
    LUA_FIELD(W, W);
    LUA_FIELD(x, X);
    LUA_FIELD(y, Y);
    LUA_FIELD(z, Z);
    LUA_FIELD(w, W);
    LUA_METHOD(Length, Length);
    LUA_METHOD(IsPoint, IsPoint);
    LUA_METHOD(IsVector, IsVector);
    LUA_METHOD(Normalize, Normalize);
    LUA_SET(Dot, [](const FVector4& Self, const FVector4& Other)
            { return Self.Dot(Other); });
    LUA_SET(Cross, [](const FVector4& Self, const FVector4& Other)
            { return Self.Cross(Other); });
    LUA_SET(Zero, []()
            { return FVector4::Zero(); });
    LUA_SET(Point, sol::overload(
                       []()
                       {
                           return FVector4::Point();
                       },
                       [](float X, float Y, float Z)
                       {
                           return FVector4::Point(X, Y, Z);
                       }));
    LUA_SET(Vector, [](float X, float Y, float Z)
            { return FVector4::Vector(X, Y, Z); });
    LUA_META(equal_to, [](const FVector4& A, const FVector4& B)
             { return A == B; });
    LUA_META(addition, [](const FVector4& A, const FVector4& B)
             { return A + B; });
    LUA_META(subtraction, [](const FVector4& A, const FVector4& B)
             { return A - B; });
    LUA_META(multiplication, sol::overload(
                                 [](const FVector4& V, float S)
                                 {
                                     return V * S;
                                 },
                                 [](float S, const FVector4& V)
                                 {
                                     return V * S;
                                 }));
    LUA_META(division, [](const FVector4& V, float S)
             { return V / S; });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FColor, "Color", []()
                           { return FColor(); }, [](float R, float G, float B, float A)
                           { return FColor(R, G, B, A); })
    LUA_FIELD(R, R);
    LUA_FIELD(G, G);
    LUA_FIELD(B, B);
    LUA_FIELD(A, A);
    LUA_FIELD(r, r);
    LUA_FIELD(g, g);
    LUA_FIELD(b, b);
    LUA_FIELD(a, a);
    LUA_METHOD(ToVector4, ToVector4);
    LUA_METHOD(ToPackedABGR, ToPackedABGR);
    LUA_SET(White, []()
            { return FColor::White(); });
    LUA_SET(Black, []()
            { return FColor::Black(); });
    LUA_SET(Red, []()
            { return FColor::Red(); });
    LUA_SET(Green, []()
            { return FColor::Green(); });
    LUA_SET(Blue, []()
            { return FColor::Blue(); });
    LUA_SET(Yellow, []()
            { return FColor::Yellow(); });
    LUA_SET(Magenta, []()
            { return FColor::Magenta(); });
    LUA_SET(Cyan, []()
            { return FColor::Cyan(); });
    LUA_SET(Gray, []()
            { return FColor::Gray(); });
    LUA_SET(Transparent, []()
            { return FColor::Transparent(); });
    LUA_SET(Lerp, [](const FColor& A, const FColor& B, float T)
            { return FColor::Lerp(A, B, T); });
    LUA_META(addition, sol::overload(
                           [](const FColor& A, const FColor& B)
                           {
                               return A + B;
                           },
                           [](const FColor& A, float V)
                           {
                               return A + V;
                           }));
    LUA_META(subtraction, sol::overload(
                              [](const FColor& A, const FColor& B)
                              {
                                  return A - B;
                              },
                              [](const FColor& A, float V)
                              {
                                  return A - V;
                              }));
    LUA_META(multiplication, sol::overload(
                                 [](const FColor& A, const FColor& B)
                                 {
                                     return A * B;
                                 },
                                 [](const FColor& A, float V)
                                 {
                                     return A * V;
                                 },
                                 [](float V, const FColor& A)
                                 {
                                     return A * V;
                                 }));
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

    LUA_BEGIN_TYPE_NO_CTOR(GLuaState, FHitResult, "HitResult")
    LUA_FIELD(Distance, Distance);
    LUA_FIELD(Location, Location);
    LUA_FIELD(Normal, Normal);
    LUA_FIELD(FaceIndex, FaceIndex);
    LUA_FIELD(bHit, bHit);
    LUA_METHOD(Reset, Reset);
    LUA_METHOD(IsValid, IsValid);
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
    LUA_SET(GetProperty, [](sol::this_state State, UObject& Object, const FString& Name)
            { return ReflectionLuaUtils::GetProperty(State, &Object, Name); });
    LUA_SET(SetProperty, [](UObject& Object, const FString& Name, const sol::object& Value)
            { return ReflectionLuaUtils::SetProperty(&Object, Name, Value); });
    LUA_END_TYPE();
}
