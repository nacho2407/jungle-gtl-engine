#include "LuaScriptComponent.h"

#include <cstring>
#include <filesystem>
#include <utility>

#include "GameFramework/World.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "GameFramework/WorldContext.h"
#include "LuaScript/LuaRuntime.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Serialization/Archive.h"

IMPLEMENT_COMPONENT_CLASS(ULuaScriptComponent, UActorComponent, EEditorComponentCategory::Scripts)

namespace
{
std::filesystem::path ResolveLuaScriptFullPath(const FString& ScriptPath)
{
    if (ScriptPath.empty() || ScriptPath == "None")
    {
        return {};
    }

    const std::filesystem::path RawPath = FPaths::ToPath(ScriptPath);
    if (RawPath.is_absolute())
    {
        return RawPath.lexically_normal();
    }

    // ScriptPath에 파일 이름만 저장됨을 가정
    if (!RawPath.has_parent_path())
    {
        return (std::filesystem::path(FPaths::ContentDir()) / L"Scripts" / FPaths::ToWide(ScriptPath)).lexically_normal();
    }

    // 상대 경로가 직접 들어온 경우도 방어적으로 지원
    return (std::filesystem::path(FPaths::RootDir()) / RawPath).lexically_normal();
}

/**
 * @brief Lua environment 안에서 특정 이름의 함수를 찾아서 가져오는 함수
 */
sol::protected_function GetOptionalLuaFunction(sol::environment& Env, const char* FunctionName, FString& LastError)
{
    sol::object Candidate = Env[FunctionName];
    if (Candidate.get_type() == sol::type::nil)
    {
        return sol::protected_function();
    }

    if (Candidate.get_type() != sol::type::function)
    {
        LastError = FString("Lua global '") + FunctionName + "' exists but is not a function.";
        UE_LOG(Lua, Warning, "%s", LastError.c_str());
        return sol::protected_function();
    }

    return Candidate.as<sol::protected_function>();
}

template <typename... TArgs>
bool CallLuaFunction(const char* FunctionName, sol::protected_function& Function, FString& OutLastError, TArgs&&... Args)
{
    if (!Function.valid())
    {
        return true;
    }

    sol::protected_function_result Result = Function(std::forward<TArgs>(Args)...);
    if (!Result.valid())
    {
        sol::error Error = Result;
        OutLastError = FString("Lua function '") + FunctionName + "' failed: " + Error.what();
        UE_LOG(Lua, Error, "%s", OutLastError.c_str());
        return false;
    }

    return true;
}
} // namespace

void ULuaScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
    if (GetWorld()->GetWorldType() == EWorldType::Editor)
    {
        return;
    }
    if (!HasScript())
    {
        return;
    }

    if (!bScriptLoaded)
    {
        if (!LoadScript())
        {
            return;
        }
    }

    CallLuaFunction("BeginPlay", BeginPlayFunc, LastError);
}

void ULuaScriptComponent::EndPlay()
{
    if (bScriptLoaded)
    {
        CallLuaFunction("EndPlay", EndPlayFunc, LastError);
    }

    ClearScriptRuntime();

    UActorComponent::EndPlay();
}

void ULuaScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    (void)TickType;
    (void)ThisTickFunction;

    if (!bScriptLoaded)
    {
        return;
    }

    CallLuaFunction("Tick", TickFunc, LastError, DeltaTime);
}

void ULuaScriptComponent::SetScriptPath(const FString& ScriptPath)
{
    if (LuaScriptPath == ScriptPath)
    {
        return;
    }

    LuaScriptPath = ScriptPath.empty() ? FString("None") : ScriptPath;
    LastError.clear();
    ClearScriptRuntime();
}

void ULuaScriptComponent::ClearScript()
{
    LuaScriptPath = "None";
    LastError.clear();
    ClearScriptRuntime();
}

bool ULuaScriptComponent::LoadScript()
{
    LastError.clear();
    ClearScriptRuntime();

    if (!HasScript())
    {
        SetLastError("ScriptPath is empty or None.");
        return false;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        OwnerActor = GetTypedOuter<AActor>();
    }

    if (!OwnerActor)
    {
        SetLastError("ULuaScriptComponent has no valid owner actor.");
        return false;
    }

    const std::filesystem::path FullPath = ResolveLuaScriptFullPath(LuaScriptPath);
    if (FullPath.empty() || !std::filesystem::exists(FullPath))
    {
        SetLastError(FString("Lua script file does not exist: ") + FPaths::FromPath(FullPath));
        return false;
    }

    sol::state& Lua = FLuaRuntime::Get().GetState();

    Env = sol::environment(Lua, sol::create, Lua.globals());

    ObjProxy.SetActor(OwnerActor);
    Env["obj"] = &ObjProxy;

    const FString FullPathUtf8 = FPaths::FromPath(FullPath);

    sol::load_result Loaded = Lua.load_file(FullPathUtf8);
    if (!Loaded.valid())
    {
        sol::error Error = Loaded;
        SetLastError(FString("Failed to load Lua script: ") + Error.what());
        return false;
    }

	// Lua.load_file()이 성공하면 파일 전체가 하나의 익명 함수 chunk처럼 로드됨
    sol::protected_function Script = Loaded;
    sol::set_environment(Env, Script);

	// 파일 실제 실행이 이 시점에서 이루어지면서 파일 내 함수 정의 등이 수행됨
    sol::protected_function_result Result = Script();
    if (!Result.valid())
    {
        sol::error Error = Result;
        SetLastError(FString("Failed to execute Lua script: ") + Error.what());
        return false;
    }

    CacheScriptFunctions();

    bScriptLoaded = true;

    UE_LOG(Lua, Info, "Loaded Lua script: %s", FullPathUtf8.c_str());
    return true;
}

bool ULuaScriptComponent::ReloadScript()
{
    if (!HasScript())
    {
        ClearScriptRuntime();
        return false;
    }

    if (LoadScript())
    {
        return true;
    }

    return false;
}

void ULuaScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << LuaScriptPath;
}

void ULuaScriptComponent::PostDuplicate()
{
    UActorComponent::PostDuplicate();

    LastError.clear();
    ClearScriptRuntime();

    // 복제 직후 에디터에서 ScriptPath만 복사해두고, 실제 Lua 실행은 BeginPlay 또는 Reload 버튼에서 수행
}

void ULuaScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    FPropertyDescriptor ScriptProp;
    ScriptProp.Name = "Script File";
    ScriptProp.Type = EPropertyType::LuaScriptRef;
    ScriptProp.ValuePtr = &LuaScriptPath;
    OutProps.push_back(ScriptProp);
}

void ULuaScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Script File") == 0)
    {
        if (!HasScript())
        {
            ClearScript();
        }
        else
        {
            ReloadScript();
        }
    }
}

void ULuaScriptComponent::ClearScriptRuntime()
{
    BeginPlayFunc = sol::protected_function();
    TickFunc = sol::protected_function();
    EndPlayFunc = sol::protected_function();

    Env = sol::environment();
    ObjProxy.SetActor(nullptr);

    bScriptLoaded = false;
}

void ULuaScriptComponent::CacheScriptFunctions()
{
    BeginPlayFunc = GetOptionalLuaFunction(Env, "BeginPlay", LastError);
    TickFunc = GetOptionalLuaFunction(Env, "Tick", LastError);
    EndPlayFunc = GetOptionalLuaFunction(Env, "EndPlay", LastError);
}

void ULuaScriptComponent::SetLastError(const FString& InError)
{
    LastError = InError;
    UE_LOG(Lua, Error, "%s", LastError.c_str());
}
