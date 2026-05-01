#include "ScriptManager.h"
#include <Windows.h>
#include <shellapi.h>
#include <UI/EditorConsoleWidget.h>

#include "GameFramework/AActor.h"
#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "Math/Vector.h"
#include "ThirdParty/sol/sol.hpp"
#include <Core/Paths.h>

namespace fs = std::filesystem;

void Log(const std::string& Msg);

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
}

void UScriptManager::intializeLuaState()
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
        sol::lib::debug,
        sol::lib::bit32,
        sol::lib::io,
        sol::lib::utf8);

	BindLuaState();
}

void UScriptManager::BindLuaState()
{
    GLuaState->new_usertype<FVector>("Vector3",
        sol::constructors<FVector(), FVector(float, float, float)>(),
        "x", &FVector::X,
        "y", &FVector::Y,
        "z", &FVector::Z,
        "size", &FVector::Size,
        "size_squared", &FVector::SizeSquared,
        "normalized", [](const FVector& Value) { return Value.GetSafeNormal(); },
        sol::meta_function::addition, sol::resolve<FVector(const FVector&) const>(&FVector::operator+),
        sol::meta_function::subtraction, sol::resolve<FVector(const FVector&) const>(&FVector::operator-),
        sol::meta_function::unary_minus, sol::resolve<FVector() const>(&FVector::operator-),
        sol::meta_function::multiplication, sol::resolve<FVector(float) const>(&FVector::operator*),
        "zero", []() { return FVector::ZeroVector; },
        "one", []() { return FVector::OneVector; },
        "forward", []() { return FVector::ForwardVector; },
        "right", []() { return FVector::RightVector; },
        "up", []() { return FVector::UpVector; }
    );

    GLuaState->new_usertype<UObject>("Object",
        "get_uuid", &UObject::GetUUID,
        "get_name", &LuaGetObjectName,
        "get_type", &LuaGetObjectType
    );

    GLuaState->new_usertype<UActorComponent>("ActorComponent",
        sol::base_classes, sol::bases<UObject>(),
        "get_owner", &UActorComponent::GetOwner,
        "is_active", &UActorComponent::IsActive,
        "set_active", &UActorComponent::SetActive,
        "is_auto_activate", &UActorComponent::IsAutoActivate,
        "set_auto_activate", &UActorComponent::SetAutoActivate,
        "is_tick_enabled", &UActorComponent::IsComponentTickEnabled,
        "set_tick_enabled", &UActorComponent::SetComponentTickEnabled,
        "is_editor_only", &UActorComponent::IsEditorOnly
    );

    GLuaState->new_usertype<USceneComponent>("SceneComponent",
        sol::base_classes, sol::bases<UActorComponent, UObject>(),
        "get_parent", &USceneComponent::GetParent,
        "attach_to", &USceneComponent::AttachToComponent,
        "get_location", &USceneComponent::GetRelativeLocation,
        "set_location", &USceneComponent::SetRelativeLocation,
        "get_rotation", &USceneComponent::GetRelativeRotation,
        "set_rotation", &USceneComponent::SetRelativeRotation,
        "get_scale", &USceneComponent::GetRelativeScale,
        "set_scale", &USceneComponent::SetRelativeScale,
        "get_world_location", &USceneComponent::GetWorldLocation,
        "set_world_location", &USceneComponent::SetWorldLocation,
        "get_world_scale", &USceneComponent::GetWorldScale,
        "get_forward", &USceneComponent::GetForwardVector,
        "get_right", &USceneComponent::GetRightVector,
        "get_up", &USceneComponent::GetUpVector,
        "move", &USceneComponent::Move,
        "move_local", &USceneComponent::MoveLocal,
        "add_world_offset", &USceneComponent::AddWorldOffset,
        "rotate", &USceneComponent::Rotate
    );

    GLuaState->new_usertype<AActor>("Actor",
        sol::base_classes, sol::bases<UObject>(),
        "get_root", &AActor::GetRootComponent,
        "get_components", &GetComponents,
        "get_component", &GetComponentByType,
        "get_location", &AActor::GetActorLocation,
        "set_location", &AActor::SetActorLocation,
        "get_rotation", &AActor::GetActorRotation,
        "set_rotation", &AActor::SetActorRotation,
        "get_scale", &AActor::GetActorScale,
        "set_scale", &AActor::SetActorScale,
        "add_world_offset", &AActor::AddActorWorldOffset,
        "get_forward", &AActor::GetActorForward,
        "is_visible", &AActor::IsVisible,
        "set_visible", &AActor::SetVisible,
        "is_active", &AActor::IsActive,
        "set_active", &AActor::SetActive,
        "should_tick_in_editor", &AActor::ShouldTickInEditor,
        "set_tick_in_editor", &AActor::SetTickInEditor
    );

    // function 바인딩 예시
    GLuaState->set_function("Log", &Log);
}

void Log(const std::string& Msg)
{
    UE_LOG("[Lua] %s", Msg.c_str());
}

bool UScriptManager::CreateScript(const FName& LuaScriptName)
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
	
	if (ScriptArray.find(ScriptName) != ScriptArray.end())
    {
        ScriptArray[ScriptName].ScriptPath = ScriptPath;
        ScriptArray[ScriptName].LastWriteTime = fs::last_write_time(ScriptPath);
    }

    return true;
}

bool UScriptManager::EditScript(const FName& LuaScriptName)
{
    if (!LuaScriptName.IsValid())
    {
        UE_LOG("[LuaManager] 선택된 스크립트가 없습니다.");
        return false;
    }

    const FString LuaScriptPath = LuaScriptName.ToString();
    HINSTANCE InstanceHandle = ShellExecuteA(nullptr, "open", LuaScriptPath.c_str(),
                                             nullptr, nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)InstanceHandle <= 32)
    {
        MessageBoxA(NULL, "Failed to open script file", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
	return true;
}

void UScriptManager::RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent)
{
	if (!ScriptComponent)
	{
		return;
	}
	FName ScriptName(name.c_str());
    if (ScriptArray.find(ScriptName) == ScriptArray.end())
    {
        return;
    }
    FLuaScriptInfo& ScriptInfo = ScriptArray[ScriptName];
	ScriptInfo.ScriptPath = FPaths::ToWide(name);
	// 이미 등록된 컴포넌트인지 확인
	auto& Components = ScriptInfo.ScriptComponents;
	auto It = std::find_if(Components.begin(), Components.end(),
						   [ScriptComponent](const std::shared_ptr<UScriptComponent>& Comp)
						   {
							   return Comp.get() == ScriptComponent;
						   });
	if (It == Components.end())
	{
		Components.push_back(std::shared_ptr<UScriptComponent>(ScriptComponent));
    }
}
