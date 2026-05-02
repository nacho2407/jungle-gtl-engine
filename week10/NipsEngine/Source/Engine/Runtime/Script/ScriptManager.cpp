#include "ScriptManager.h"
#include "ScriptComponent.h"
#include <Windows.h>
#include <shellapi.h>

#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "Component/ActorComponent.h"
#include "Component/MeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"
#include "Component/DecalComponent.h"
#include "Asset/StaticMesh.h"
#include "Math/Vector.h"
#include "ThirdParty/sol/sol.hpp"
#include "ScriptUtils.h"
#include <Core/Paths.h>

namespace fs = std::filesystem;

void Log(const std::string& Msg);

namespace
{
    FName MakeLuaScriptKey(const FString& ScriptName)
    {
        std::filesystem::path ScriptPath(FPaths::ToWide(ScriptName));
        FString Key = FPaths::ToUtf8(ScriptPath.stem().generic_wstring());
        if (Key.empty())
        {
            Key = ScriptName;
        }
        return FName(Key);
    }

    FString LuaGetObjectName(UObject& Object)
    {
        return Object.GetName();
    }

    FString LuaGetObjectType(UObject& Object)
    {
        const FTypeInfo* TypeInfo = Object.GetTypeInfo();
        return TypeInfo ? TypeInfo->name : "";
    }

    UActorComponent* GetComponentByType(AActor& Actor, const FString& TypeName)
    {
        for (UActorComponent* Component : Actor.GetComponents())
        {
            if (!Component)
            {
                continue;
            }

            const FTypeInfo* TypeInfo = Component->GetTypeInfo();
            if (TypeInfo && TypeName == TypeInfo->name)
            {
                return Component;
            }
        }

        return nullptr;
    }

    UStaticMeshComponent* GetStaticMeshComponent(AActor& Actor)
    {
        for (UActorComponent* Component : Actor.GetComponents())
        {
            if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
            {
                return StaticMeshComponent;
            }
        }

        return nullptr;
    }

    sol::table GetComponents(sol::this_state State, AActor& Actor)
    {
        sol::state_view Lua(State);
        sol::table Components = Lua.create_table();

        int32 Index = 1;
        for (UActorComponent* Component : Actor.GetComponents())
        {
            if (Component)
            {
                Components[Index++] = Component;
            }
        }

        return Components;
    }

	static std::string FileTimeToString(std::filesystem::file_time_type FileTime)
    {
        using namespace std::chrono;

        auto SystemTime = time_point_cast<system_clock::duration>(
            FileTime - std::filesystem::file_time_type::clock::now() + system_clock::now());

        std::time_t Time = system_clock::to_time_t(SystemTime);

        std::tm LocalTime{};
        localtime_s(&LocalTime, &Time);

        std::ostringstream Oss;
        Oss << std::put_time(&LocalTime, "%Y-%m-%d %H:%M:%S");

        return Oss.str();
    }
}

void FScriptManager::BindMathTypes() {
    // ============================================================
    // Log
    // ============================================================

    GLuaState->set_function("Log", [](const std::string& Msg)
                        { UE_LOG(("[Lua] " + Msg + "\n").c_str()); });

    GLuaState->set_function("LogWarning", [](const std::string& Msg)
                        { UE_LOG(("[Lua Warning] " + Msg + "\n").c_str()); });

    GLuaState->set_function("LogError", [](const std::string& Msg)
                        { UE_LOG(("[Lua Error] " + Msg + "\n").c_str()); });


    // ============================================================
    // FName
    // ============================================================

    LUA_BEGIN_TYPE_FACTORY(GLuaState, FName, "FName", []()
                           { return FName(); }, [](const char* Str)
                           { return FName(Str); }, [](const std::string& Str)
                           { return FName(Str.c_str()); })
    LUA_METHOD(ToString, ToString);

    LUA_META(to_string, [](const FName& Name)
             { return Name.ToString(); });

    LUA_END_TYPE();


    // ============================================================
    // FVector
    // ============================================================

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


    // ============================================================
    // FQuat
    // ============================================================

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


    // ============================================================
    // FTransform
    // ============================================================

	LUA_BEGIN_TYPE_FACTORY(GLuaState, FTransform, "Transform",
    []()
    {
        return FTransform();
    },
    [](const FVector& Translation, const FQuat& Rotation, const FVector& Scale)
    {
        // Lua 쪽 인자 순서는 Translation, Rotation, Scale로 유지
        // C++ 생성자는 Rotation, Translation, Scale 순서
        return FTransform(Rotation, Translation, Scale);
    })

    LUA_PROPERTY(Location,
                 &FTransform::GetLocation,
                 &FTransform::SetLocation);

    LUA_PROPERTY(Translation,
                 &FTransform::GetTranslation,
                 &FTransform::SetTranslation);

    LUA_PROPERTY(Rotation,
                 &FTransform::GetRotation,
                 [](FTransform& Self, const FQuat& InRotation)
                 {
                     Self.SetRotation(InRotation);
                 });

    LUA_PROPERTY(Scale,
                 &FTransform::GetScale3D,
                 &FTransform::SetScale3D);

    LUA_END_TYPE();
}

void FScriptManager::BindObjectTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR(GLuaState, UObject, "Object")
    LUA_RO_PROPERTY(UUID, GetUUID);
    LUA_METHOD(GetUUID, GetUUID);
    LUA_END_TYPE();
}

void FScriptManager::BindComponentTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorComponent, "ActorComponent", UObject)
    LUA_METHOD(GetOwner, GetOwner);
    LUA_METHOD(IsActive, IsActive);
    LUA_METHOD(SetActive, SetActive);
    LUA_METHOD(IsAutoActivate, IsAutoActivate);
    LUA_METHOD(SetAutoActivate, SetAutoActivate);
    LUA_METHOD(IsComponentTickEnabled, IsComponentTickEnabled);
    LUA_METHOD(SetComponentTickEnabled, SetComponentTickEnabled);
    LUA_METHOD(IsEditorOnly, IsEditorOnly);
    LUA_RO_PROPERTY(Owner, GetOwner);
    LUA_RW_PROPERTY(Active, IsActive, SetActive);
    LUA_RW_PROPERTY(AutoActivate, IsAutoActivate, SetAutoActivate);
    LUA_RW_PROPERTY(TickEnabled, IsComponentTickEnabled, SetComponentTickEnabled);
    LUA_RO_PROPERTY(EditorOnly, IsEditorOnly);
    LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USceneComponent, "SceneComponent", UActorComponent, UObject)
    LUA_METHOD(GetParent, GetParent);
    LUA_METHOD(AttachToComponent, AttachToComponent);

    LUA_METHOD(GetRelativeLocation, GetRelativeLocation);
    LUA_METHOD(SetRelativeLocation, SetRelativeLocation);
    LUA_RW_PROPERTY(Location, GetRelativeLocation, SetRelativeLocation);
    LUA_RO_PROPERTY(Forward, GetForwardVector);
    LUA_END_TYPE();
}

void FScriptManager::BindActorTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, AActor, "Actor", UObject)
    LUA_PROPERTY(Name, [](AActor& Actor) -> FString
                 { return Actor.GetName(); });
    LUA_RW_PROPERTY(Location, GetActorLocation, SetActorLocation);
    LUA_RW_PROPERTY(Rotation, GetActorRotation, SetActorRotation);
    LUA_RW_PROPERTY(Scale, GetActorScale, SetActorScale);
    LUA_RO_PROPERTY(UUID, GetUUID);
    LUA_RO_PROPERTY(RootComponent, GetRootComponent);
    LUA_RW_PROPERTY(Active, IsActive, SetActive);
    LUA_RW_PROPERTY(Visible, IsVisible, SetVisible);
    LUA_RW_PROPERTY(TickInEditor, ShouldTickInEditor, SetTickInEditor);
    LUA_METHOD(Duplicate, Duplicate);
    LUA_METHOD(GetActorForwardVector, GetActorForward);

    LUA_METHOD(Add_Actor_World_Offset, AddActorWorldOffset);
    LUA_METHOD(Get_Actor_Forward, GetActorForward);

    LUA_SET(Get_Components, &GetComponents);
    LUA_SET(Get_Component_By_Type, &GetComponentByType);
    LUA_SET(Get_Static_Mesh_Component, &GetStaticMeshComponent);
    LUA_END_TYPE();
}

void FScriptManager::BindStaticMeshTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UStaticMesh, "StaticMesh", UObject)
    LUA_METHOD(GetAssetPath, GetAssetPathFileName);
    LUA_METHOD(HasValidMesh, HasValidMeshData);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UStaticMeshComponent, "StaticMeshComponent", UMeshComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject)
    LUA_METHOD(GetStaticMesh, GetStaticMesh);
    LUA_METHOD(SetStaticMesh, SetStaticMesh);
    LUA_METHOD(HasValidMesh, HasValidMesh);
    LUA_METHOD(GetPrimitiveType, GetPrimitiveType);
    LUA_END_TYPE();
}

void FScriptManager::BindBillboardTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UBillboardComponent, "BillboardComponent", USceneComponent, UActorComponent, UObject)
    LUA_METHOD(SetBillboardEnabled, SetBillboardEnabled);
    LUA_METHOD(SetTextureName, SetTextureName);
    LUA_METHOD(GetTexture, GetTexture);
    LUA_END_TYPE();
}

void FScriptManager::BindCameraTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UCameraComponent, "CameraComponent",
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_METHOD(look_at, LookAt);

    LUA_RW_PROPERTY(FOV, GetFOV, SetFOV);
    LUA_RW_PROPERTY(OrthoWidth, GetOrthoWidth, SetOrthoWidth);
    LUA_RW_PROPERTY(Orthographic, IsOrthogonal, SetOrthographic);

    LUA_RO_PROPERTY(NearPlane, GetNearPlane);
    LUA_RO_PROPERTY(FarPlane, GetFarPlane);

    LUA_METHOD(get_view_matrix, GetViewMatrix);
    LUA_METHOD(get_projection_matrix, GetProjectionMatrix);

    LUA_METHOD(move_forward, MoveForward);
    LUA_METHOD(move_right, MoveRight);
    LUA_METHOD(move_up, MoveUp);

    LUA_METHOD(add_yaw_input, AddYawInput);
    LUA_METHOD(add_pitch_input, AddPitchInput);

    LUA_RO_PROPERTY(Forward, GetForwardVector);
    LUA_RO_PROPERTY(Right, GetRightVector);
    LUA_RO_PROPERTY(Up, GetUpVector);

    LUA_END_TYPE();
}

void FScriptManager::BindPrimitiveTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UPrimitiveComponent, "PrimitiveComponent",
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_RW_PROPERTY(Visible, IsVisible, SetVisibility);
    LUA_RW_PROPERTY(EnableCull, IsEnableCull, SetEnableCull);

    LUA_RO_PROPERTY(GenerateOverlapEvents, ShouldGenerateOverlapEvents);
    LUA_RO_PROPERTY(NumMaterials, GetNumMaterials);
    LUA_RO_PROPERTY(SupportsOutline, SupportsOutline);

    LUA_METHOD(is_overlapping_actor, IsOverlappingActor);
    LUA_METHOD(clear_overlaps, ClearOverlaps);

    LUA_END_TYPE();
}

void FScriptManager::BindDecalTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UDecalComponent, "DecalComponent",
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_METHOD(SetSize, SetSize);

    LUA_METHOD(SetFadeIn, SetFadeIn);
    LUA_METHOD(SetFadeOut, SetFadeOut);

    LUA_METHOD(GetDecalMatrix, GetDecalMatrix);
    LUA_METHOD(GetDecalColor, GetDecalColor);

    LUA_SET(GetMaterial, [](UDecalComponent& Self)
            { return Self.GetMaterial(); });

    LUA_SET(SetMaterial, [](UDecalComponent& Self, UMaterialInterface* Material)
            { Self.SetMaterial(Material); });

    LUA_RO_PROPERTY(DecalMatrix, GetDecalMatrix);
    LUA_RO_PROPERTY(DecalColor, GetDecalColor);
    LUA_RO_PROPERTY(NumMaterials, GetNumMaterials);

    LUA_END_TYPE();
}

void FScriptManager::initializeLuaState()
{
    GLuaState = std::make_unique<sol::state>();
    GLuaState->open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::os,
        sol::lib::math,
        sol::lib::table,
        sol::lib::bit32,
        sol::lib::utf8);


	RefreshLuaScriptFiles();
	BindLuaState();
}

void FScriptManager::BindLuaState()
{
    BindMathTypes();
    BindComponentTypes();
    BindActorTypes();
    // function 바인딩 예시
    GLuaState->set_function("Log", &Log);
}

void Log(const std::string& Msg)
{
    UE_LOG("[Lua] %s", Msg.c_str());
}

bool FScriptManager::CreateScript(const FName& LuaScriptName)
{
    FString ScriptName;
    if (!LuaScriptName.IsValid())
    {
        ScriptName = "Actor.lua";
    }
    else
    {
        ScriptName = LuaScriptName.ToString();

        if (!ScriptName.ends_with(".lua"))
        {
            ScriptName += ".lua";
        }
    }

    FWString TemplatePath = FPaths::LuaTemplatePath();
    FWString ScriptPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptName));

    if (!fs::exists(TemplatePath))
    {
        MessageBoxW(nullptr, TemplatePath.c_str(), L"Path", MB_OK);
        UE_LOG("[LuaManager] 템플릿 파일이 존재하지 않습니다: %s", TemplatePath.c_str());
        return false;
    }

    if (fs::exists(ScriptPath))
    {
        UE_LOG("[LuaManager] 스크립트 파일이 이미 존재합니다: %s", ScriptPath.c_str());
        return false;
    }

    try
    {
        fs::copy_file(
            TemplatePath,
            ScriptPath,
            fs::copy_options::none);
    }
    catch (const fs::filesystem_error& e)
    {
        UE_LOG("[LuaManager] 스크립트 복사 실패: %s", e.what());
        return false;
    }
	
    const FName ScriptKey = MakeLuaScriptKey(ScriptName);
    if (ScriptArray.find(ScriptKey) != ScriptArray.end())
    {
        ScriptArray[ScriptKey].ScriptPath = ScriptPath;
        ScriptArray[ScriptKey].LastWriteTime = fs::last_write_time(ScriptPath);
    }

	RefreshLuaScriptFiles();
    UE_LOG("[LuaManager] 스크립트 생성 완료: %s", ScriptPath.c_str());
    return true;
}

bool FScriptManager::EditScript(const FName& LuaScriptName)
{
    if (!LuaScriptName.IsValid())
    {
        UE_LOG("[LuaManager] 선택된 스크립트가 없습니다.");
        return false;
    }

	FWString ScriptPath;
    const FName ScriptKey = MakeLuaScriptKey(LuaScriptName.ToString());
	if (ScriptArray.find(ScriptKey) != ScriptArray.end())
    {
        ScriptPath = ScriptArray[ScriptKey].ScriptPath;
    }
	else
    {
        FString ScriptName;
        if (!LuaScriptName.IsValid())
        {
            ScriptName = "Actor.lua";
        }
        else
        {
            ScriptName = LuaScriptName.ToString();

            if (!ScriptName.ends_with(".lua"))
            {
                ScriptName += ".lua";
            }
        }

        ScriptPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptName));
    }

    HINSTANCE InstanceHandle = ShellExecute(nullptr, L"open", ScriptPath.data(),
                                             nullptr, nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)InstanceHandle <= 32)
    {
        MessageBoxW(NULL, ScriptPath.c_str(), L"Path", MB_OK | MB_ICONERROR);
        UE_LOG("[LuaManager] 스크립트 파일을 열 수 없습니다: %s", ScriptPath.c_str());
        return false;
    }
	return true;
}

bool FScriptManager::HasScript(const FName& name)
{
    auto It = ScriptArray.find(MakeLuaScriptKey(name.ToString()));

    if (It == ScriptArray.end())
    {
        return false;
    }
    return true;
}

void FScriptManager::HotReloadScripts()
{
    RefreshLuaScriptFiles();

	for (auto& [ScriptKey, ScriptInfo] : ScriptArray)
	{
		if (!ScriptInfo.ScriptPath.empty() && fs::exists(ScriptInfo.ScriptPath))
		{
			auto LastWriteTime = fs::last_write_time(ScriptInfo.ScriptPath);

			if (LastWriteTime > ScriptInfo.LastWriteTime)
            {
                UE_LOG("[ScriptManager] 핫 리로드 실행");
                bool bReloadSuccess = true;
                for (auto * ScriptComponent : ScriptInfo.ScriptComponents)
				{
					if (!ScriptComponent) continue;

                    UE_LOG("[ScriptManager] 핫리로드 대상 %s", ScriptComponent->GetName().c_str());
                    bool bResult = ScriptComponent->HotReloadScript();
                    bReloadSuccess = bReloadSuccess && bResult;
                }
                if(bReloadSuccess) ScriptInfo.LastWriteTime = LastWriteTime;
			}
		}
	}
}

void FScriptManager::RefreshLuaScriptFiles()
{
	FWString ScriptDir = FPaths::ScriptDir();
	if (!fs::exists(ScriptDir))
	{
		return;
	}
	for (const auto& Entry : fs::directory_iterator(ScriptDir))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
		{
            FString ScriptName = FPaths::ToUtf8(Entry.path().stem().generic_wstring());
            FName ScriptKey = MakeLuaScriptKey(ScriptName);

            FLuaScriptInfo& Info = ScriptArray[ScriptKey];
            Info.ScriptPath = Entry.path().wstring();
			if (Info.LastWriteTime == fs::file_time_type::min())
			{
				Info.LastWriteTime = fs::last_write_time(Entry.path());
            }
		}
    }
}

void FScriptManager::RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent)
{
    if (!ScriptComponent)
    {
        return;
    }

    FName ScriptName = MakeLuaScriptKey(name);

    FLuaScriptInfo& ScriptInfo = ScriptArray[ScriptName];

    if (ScriptInfo.ScriptPath.empty())
    {
        FString FileName = name.ends_with(".lua") ? name : name + ".lua";
        ScriptInfo.ScriptPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(FileName));
    }

    auto& Components = ScriptInfo.ScriptComponents;

    auto It = std::find(
        Components.begin(),
        Components.end(),
        ScriptComponent);

    if (It == Components.end())
    {
        Components.push_back(ScriptComponent);
    }
}

void FScriptManager::UnregisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent)
{
    if (!ScriptComponent)
        return;

    FName ScriptName = MakeLuaScriptKey(name);
    auto It = ScriptArray.find(ScriptName);
	if (It == ScriptArray.end())
	{
		return;
	}
	auto& Components = It->second.ScriptComponents;

	Components.erase(
		std::remove(Components.begin(), Components.end(), ScriptComponent),
        Components.end());
}

void FScriptManager::UnregisterScriptComponentAll(UScriptComponent* ScriptComponent)
{
    if (!ScriptComponent)
        return;

    for (auto It = ScriptArray.begin(); It != ScriptArray.end();)
    {
        auto& Components = It->second.ScriptComponents;

        Components.erase(
            std::remove(Components.begin(), Components.end(), ScriptComponent),
            Components.end());

        ++It;
    }
}

std::optional<sol::environment> FScriptManager::LoadLuaEnvironment(UScriptComponent* ScriptComponent, const FString& ScriptName)
{
    if (!GLuaState)
    {
        UE_LOG("[LuaManager] Lua state is not initialized.");
        return std::nullopt;
    }

    //스크립트가 추가되었을 수 있으므로 한번 초기화
    RefreshLuaScriptFiles();

    const FName ScriptKey = MakeLuaScriptKey(ScriptName);
    auto It = ScriptArray.find(ScriptKey);
    if (It == ScriptArray.end())
    {
        UE_LOG("[LuaManager] 스크립트를 찾을 수 없습니다: %s", ScriptName.c_str());
        return std::nullopt;
    }

	//ScriptPath 저장
    FWString ScriptPath = It->second.ScriptPath;
    if (ScriptPath.empty() || !fs::exists(ScriptPath))
    {
        UE_LOG("[LuaManager] 스크립트 파일이 존재하지 않습니다: %s", ScriptPath.c_str());
        return std::nullopt;
    }

    sol::environment Env(*GLuaState, sol::create, GLuaState->globals());

    if (ScriptComponent)
    {
        Env["Self"] = ScriptComponent;
        Env["Actor"] = ScriptComponent->GetOwner();
    }

    auto Result = GLuaState->safe_script_file(
        FPaths::ToUtf8(ScriptPath),
        Env,
        sol::script_pass_on_error);

    if (!Result.valid())
    {
        sol::error Err = Result;

        UE_LOG("[Lua Load Error] Script: %s, Error: %s",
               ScriptName.c_str(),
               Err.what());

        return std::nullopt;
    }

    return Env;
}

auto FScriptManager::GetScriptInfo(const FName& name) -> FLuaScriptInfo*
{
    auto It = ScriptArray.find(MakeLuaScriptKey(name.ToString()));

    if (It == ScriptArray.end())
    {
        return nullptr;
    }

    return &It->second;
}
