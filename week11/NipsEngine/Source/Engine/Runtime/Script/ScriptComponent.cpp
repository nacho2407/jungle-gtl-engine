#include "ScriptComponent.h"
#include "ScriptManager.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Paths.h"
#include "Core/CollisionTypes.h"
#include <algorithm>

DEFINE_CLASS(UScriptComponent, UActorComponent)
REGISTER_FACTORY(UScriptComponent)

namespace
{
    void BindCoroutineHelpers(sol::environment& Env, UScriptComponent* ScriptComponent)
    {
        if (!Env.valid() || !ScriptComponent)
        {
            return;
        }

        Env["StartCoroutine"] = [ScriptComponent](sol::function Function)
        {
            ScriptComponent->StartCoroutine(Function);
        };

        Env["WaitForSeconds"] = [](sol::this_state State, float Seconds)
        {
            sol::state_view Lua(State);
            sol::table Table = Lua.create_table();
            Table["type"] = "seconds";
            Table["value"] = Seconds;
            return Table;
        };

        Env["WaitForFrames"] = [](sol::this_state State, int Frames)
        {
            sol::state_view Lua(State);
            sol::table Table = Lua.create_table();
            Table["type"] = "frames";
            Table["value"] = Frames;
            return Table;
        };
    }

	EPropertyType ParseLuaScriptPropertyType(const FString& TypeName)
    {
        if (TypeName == "Int")
            return EPropertyType::Int;
        if (TypeName == "Float")
            return EPropertyType::Float;
        if (TypeName == "Bool")
            return EPropertyType::Bool;
        if (TypeName == "String")
            return EPropertyType::String;
        if (TypeName == "Vector")
            return EPropertyType::Vec3;

        return EPropertyType::String;
    }

	FLuaScriptProperty* FindPropertyInArray(TArray<FLuaScriptProperty>& Properties, const FString& Name)
	{
		for (FLuaScriptProperty& Prop : Properties)
		{
			if (Prop.Name == Name)
			{
				return &Prop;
			}
		}
        return nullptr;
    }

    void* GetLuaPropertyValuePtr(FLuaScriptProperty& Prop)
    {
        switch (Prop.Type)
        {
        case EPropertyType::Int:
            return &Prop.IntValue;

        case EPropertyType::Float:
            return &Prop.FloatValue;

        case EPropertyType::Bool:
            return &Prop.BoolValue;

        case EPropertyType::String:
            return &Prop.StringValue;

        case EPropertyType::Vec3:
            return &Prop.Vec3Value;

        default:
            return nullptr;
        }
    }

	static bool MakeLuaPropertyDescriptor(
        FLuaScriptProperty& LuaProp,
        FPropertyDescriptor& OutDesc)
    {
        void* ValuePtr = GetLuaPropertyValuePtr(LuaProp);
        if (!ValuePtr)
        {
            return false;
        }

        OutDesc.Name = LuaProp.Name.c_str();
        OutDesc.Type = LuaProp.Type;
        OutDesc.ValuePtr = ValuePtr;

        OutDesc.Min = LuaProp.bHasMin ? LuaProp.Min : 0.0f;
        OutDesc.Max = LuaProp.bHasMax ? LuaProp.Max : 0.0f;
        OutDesc.Speed = 0.1f;

        OutDesc.EnumNames = nullptr;
        OutDesc.EnumCount = 0;
        OutDesc.ExtraData = nullptr;

        return true;
    }
    }

void UScriptComponent::PostDuplicate(UObject* Original)
{
    UActorComponent::PostDuplicate(Original);

    UScriptComponent* Source = Cast<UScriptComponent>(Original);
    if (!Source)
    {
        return;
    }

    ScriptName = Source->ScriptName;
    LuaProperties = Source->LuaProperties;

    ClearLoadedState();
    bScriptRegistered = false;

    if (!ScriptName.empty() && FScriptManager::Get().HasScript(ScriptName))
    {
        ReloadLuaProperties();
    }
}

UScriptComponent::~UScriptComponent()
{
    UnregisterScript();
    ReleaseLuaStateReferences();
}

void UScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << "ScriptName" << ScriptName;

    int32 LuaPropertyCount = static_cast<int32>(LuaProperties.size());
    Ar << "LuaPropertyCount" << LuaPropertyCount;

    if (Ar.IsLoading())
    {
        LuaProperties.clear();
        LuaProperties.resize(LuaPropertyCount);
    }

    for (int32 i = 0; i < LuaPropertyCount; ++i)
    {
        FLuaScriptProperty& Prop = LuaProperties[i];

        Ar << "Name" << Prop.Name;
        Ar << "TypeName" << Prop.TypeName;
        Ar << "Category" << Prop.Category;

        if (Ar.IsLoading())
        {
            Prop.Type = ParseLuaScriptPropertyType(Prop.TypeName);
        }

        switch (Prop.Type)
        {
        case EPropertyType::Int:
            Ar << "IntValue" << Prop.IntValue;
            break;

        case EPropertyType::Float:
            Ar << "FloatValue" << Prop.FloatValue;
            break;

        case EPropertyType::Bool:
            Ar << "BoolValue" << Prop.BoolValue;
            break;

        case EPropertyType::String:
            Ar << "StringValue" << Prop.StringValue;
            break;

        case EPropertyType::Vec3:
            Ar << "Vec3Value" << Prop.Vec3Value;
            break;

        default:
            break;
        }

        Ar << "bHasMin" << Prop.bHasMin;
        Ar << "bHasMax" << Prop.bHasMax;
        Ar << "Min" << Prop.Min;
        Ar << "Max" << Prop.Max;
    }

    if (Ar.IsLoading() && !ScriptName.empty())
    {
        ReloadLuaProperties();
    }
}
void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "ScriptName", EPropertyType::String, &ScriptName });

	// ScriptName이 바뀐 뒤 아직 Property를 안 읽었다면 여기서 갱신
    if (!ScriptName.empty() && !bLuaPropertiesScanned)
    {
        ReloadLuaProperties();
    }

	for (FLuaScriptProperty& LuaProp : LuaProperties)
    {
        FPropertyDescriptor Desc{};
        if (MakeLuaPropertyDescriptor(LuaProp, Desc))
        {
            OutProps.push_back(Desc);
        }
    }
}

void UScriptComponent::SetScriptName(const FString& InScriptName)
{
    if (ScriptName == InScriptName)
    {
        return;
    }

    UnregisterScript();

    ScriptName = InScriptName;
    ClearLoadedState();
    bLuaPropertiesScanned = false;

	ReloadLuaProperties();
}

bool UScriptComponent::RegisterScript()
{
    if (ScriptName.empty())
    {
        return false;
    }

    if (bScriptRegistered)
    {
        return true;
    }

    if (!FScriptManager::Get().HasScript(ScriptName))
    {
        return false;
    }

    FScriptManager::Get().RegisterScriptComponents(ScriptName, this);
    bScriptRegistered = true;

    return true;
}

void UScriptComponent::UnregisterScript()
{
    if (!bScriptRegistered)
    {
        return;
    }

    FScriptManager::Get().UnregisterScriptComponents(ScriptName, this);
    bScriptRegistered = false;
}

void UScriptComponent::ClearScript()
{
    UnregisterScript();

    ScriptName.clear();
    ClearLoadedState();
}

void UScriptComponent::ReleaseLuaStateReferences()
{
    bScriptRegistered = false;
    ClearLoadedState();
}

bool UScriptComponent::LoadScript()
{
    if (ScriptName.empty())
    {
        ClearLoadedState();
        return false;
    }

    auto Loaded = FScriptManager::Get().LoadScriptClass(this, ScriptName);
    if (Loaded == std::nullopt)
    {
        ClearLoadedState();
        return false;
    }

    BindCoroutineHelpers(Loaded->Env, this);
	auto Instance = CreateScriptInstance(*Loaded);
    if (!Instance)
    {
        ClearLoadedState();
		return false;
    }
    CoroutineScheduler.StopAll();

    ScriptEnv = std::move(Loaded->Env);
    ScriptClass = Loaded->ScriptClass;
    ScriptInstance = *Instance;
    BindCoroutineHelpers(ScriptEnv, this);
    bScriptLoaded = true;
    RegisterScript();

    return true;
}

bool UScriptComponent::HotReloadScript()
{
    if (ScriptName.empty())
    {
        return false;
    }

	auto Loaded = FScriptManager::Get().LoadScriptClass(this, ScriptName);
    if (Loaded == std::nullopt)
    {
        return false;
    }

    BindCoroutineHelpers(Loaded->Env, this);
    auto Instance = CreateScriptInstance(*Loaded);
    if (!Instance)
    {
        return false;
    }

    CoroutineScheduler.StopAll();

    ScriptEnv = std::move(Loaded->Env);
    ScriptClass = Loaded->ScriptClass;
    ScriptInstance = *Instance;

    BindCoroutineHelpers(ScriptEnv, this);
    bScriptLoaded = true;

    return true;
}

void UScriptComponent::StartCoroutine(sol::function Function)
{
    if (!Function.valid())
    {
        UE_LOG_ERROR("[ScriptComponent] Invalid coroutine function in script '%s'", ScriptName.c_str());
        return;
    }

    CoroutineScheduler.StartCoroutine(Function);
}

void UScriptComponent::OnUnregister()
{ 
	// 부모 훅 호출
    UActorComponent::OnUnregister();

    // 스크립트가 등록되어있다면 매니저에 언레지스터 요청
    UnregisterScript();
}

// Lifecycle 훅에서 Lua 함수 호출. Lua 환경이 유효한 경우에만 시도합니다.
void UScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

    if (!bScriptLoaded)
    {
        if (!LoadScript())
        {
            return;
        }
    }
	CallScriptFunction("BeginPlay");
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    UActorComponent::TickComponent(DeltaTime);

    if (!bScriptLoaded)
    {
        return;
    }

    CallScriptFunction("Tick", DeltaTime);
    CoroutineScheduler.Tick(DeltaTime);
}

void UScriptComponent::EndPlay()
{
    UActorComponent::EndPlay();

	if (!ScriptEnv.valid())
	{
        CoroutineScheduler.StopAll();
		return;
	}

    CallScriptFunction("EndPlay");
    CoroutineScheduler.StopAll();
}

sol::optional<sol::table> UScriptComponent::CreateScriptInstance(const FLuaScriptLoadResult& Loaded)
{
    sol::state* Lua = FScriptManager::Get().GetGlobalLuaState();
    if (!Lua)
    {
        UE_LOG_ERROR("[ScriptComponent] LuaState is null");
        return std::nullopt;
    }

    sol::table RuntimeProperties = MakeRuntimePropertyTable(*Lua);

    sol::object NewObj = Loaded.ScriptClass["new"];
    if (!NewObj.valid() || NewObj.get_type() != sol::type::function)
    {
        UE_LOG_ERROR("[ScriptComponent] Script.new missing: %s", ScriptName.c_str());
        return std::nullopt;
    }

    sol::protected_function NewFunc = NewObj.as<sol::protected_function>();

    sol::protected_function_result NewResult =
        NewFunc(this, RuntimeProperties);

    if (!NewResult.valid())
    {
        sol::error Err = NewResult;
        UE_LOG_ERROR("[ScriptComponent] Script.new failed: %s", Err.what());
        return std::nullopt;
    }

    sol::object InstanceObj = NewResult;

    if (!InstanceObj.valid() || InstanceObj.get_type() != sol::type::table)
    {
        UE_LOG_ERROR("[ScriptComponent] Script.new must return table: %s", ScriptName.c_str());
        return std::nullopt;
    }

    return InstanceObj.as<sol::table>();
}

// Lua 스크립트에서 정의된 Properties 테이블을 읽어서 LuaProperties 배열을 갱신
void UScriptComponent::ReloadLuaProperties()
{
    // 1. 기존 값 백업
    TArray<FLuaScriptProperty> OldProperties = LuaProperties;

    // 2. 중복 방지를 위해 clear
    LuaProperties.clear();
    bLuaPropertiesScanned = false;

    if (ScriptName.empty())
    {
        return;
    }

    if (!FScriptManager::Get().HasScript(ScriptName))
    {
        UE_LOG_WARNING("[ScriptComponent] Script not found: %s", ScriptName.c_str());
        return;
    }

    // 여기서부터 실제 파일 로드 성공하면 true로 둘 준비
    auto LoadedClass = FScriptManager::Get().LoadScriptClassForProperties(ScriptName);
    if (!LoadedClass)
    {
        UE_LOG_WARNING("[ScriptComponent] property read failed: %s", ScriptName.c_str());
        return;
    }

    bLuaPropertiesScanned = true;

    sol::table ScriptClassTable = *LoadedClass;
    sol::object PropsObj = ScriptClassTable["Properties"];

    if (!PropsObj.valid() || PropsObj.get_type() != sol::type::table)
    {
        LuaProperties.clear();
        return;
    }

    sol::table PropsTable = PropsObj.as<sol::table>();

    for (const auto& Pair : PropsTable)
    {
        sol::object KeyObj = Pair.first;
        sol::object ValueObj = Pair.second;
		if (!KeyObj.valid() || KeyObj.get_type() != sol::type::string)
		{
			continue;
        }
        if (ValueObj.get_type() != sol::type::table)
        {
            continue;
        }

		FString PropName = KeyObj.as<std::string>();
        sol::table PropTable = ValueObj.as<sol::table>();

		FString TypeName = PropTable["Type"].get_or<FString>("String");
        EPropertyType PropType = ParseLuaScriptPropertyType(TypeName);

		FLuaScriptProperty NewProp;
		NewProp.Name = PropName;
        NewProp.TypeName = TypeName;
        NewProp.Type = PropType;
        NewProp.Category = PropTable["Category"].get_or<FString>("Default");

        //기존 값이 있으면 보존
        FLuaScriptProperty* OldProp = FindPropertyInArray(OldProperties, PropName);

        if (OldProp != nullptr && OldProp->Type == PropType)
        {
            NewProp.IntValue = OldProp->IntValue;
            NewProp.FloatValue = OldProp->FloatValue;
            NewProp.BoolValue = OldProp->BoolValue;
            NewProp.StringValue = OldProp->StringValue;
        }
        else
        {
            //기존 값이 없거나 타입이 바뀌었으면 Default 사용
            switch (PropType)
            {
            case EPropertyType::Int:
                NewProp.IntValue = PropTable.get_or("Default", 0);
                break;

            case EPropertyType::Float:
                NewProp.FloatValue = PropTable.get_or("Default", 0.0f);
                break;

            case EPropertyType::Bool:
                NewProp.BoolValue = PropTable.get_or("Default", false);
                break;

            case EPropertyType::String:
                NewProp.StringValue = PropTable.get_or("Default", std::string(""));
                break;
            }
        }

        if (PropTable["Min"].valid())
        {
            NewProp.bHasMin = true;
            NewProp.Min = PropTable["Min"].get<float>();
        }

        if (PropTable["Max"].valid())
        {
            NewProp.bHasMax = true;
            NewProp.Max = PropTable["Max"].get<float>();
        }

        LuaProperties.push_back(NewProp);
	}

	std::sort(
        LuaProperties.begin(),
        LuaProperties.end(),
        [](const FLuaScriptProperty& A, const FLuaScriptProperty& B)
        {
            return A.TypeName > B.TypeName;
        });
}

sol::table UScriptComponent::MakeRuntimePropertyTable(sol::state& Lua)
{
    sol::table Table = Lua.create_table();

    for (const FLuaScriptProperty& Prop : LuaProperties)
    {
        const std::string Name = Prop.Name.c_str();

        switch (Prop.Type)
        {
        case EPropertyType::Int:
            Table[Name] = Prop.IntValue;
            break;

        case EPropertyType::Float:
            Table[Name] = Prop.FloatValue;
            break;

        case EPropertyType::Bool:
            Table[Name] = Prop.BoolValue;
            break;

        case EPropertyType::String:
            Table[Name] = std::string(Prop.StringValue.c_str());
            break;

        case EPropertyType::Vec3:
            Table[Name] = Prop.Vec3Value;
            break;

        default:
            break;
        }
    }

    return Table;
}


void UScriptComponent::ClearLoadedState()
{
    bScriptLoaded = false;
    CoroutineScheduler.StopAll();

    ScriptEnv = sol::environment{};
    ScriptClass = sol::table{};
    ScriptInstance = sol::table{};
}

void UScriptComponent::OnHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    CallScriptFunction(
        "OnHit",
        HitComponent,
        OtherActor,
        OtherComp,
        NormalImpulse,
        Hit);
}

void UScriptComponent::OnBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    CallScriptFunction(
        "OnBeginOverlap",
        OverlappedComponent,
        OtherActor,
        OtherComp,
        OtherBodyIndex,
        bFromSweep,
        SweepResult);
}

void UScriptComponent::OnEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    CallScriptFunction(
        "OnEndOverlap",
        OverlappedComponent,
        OtherActor,
        OtherComp,
        OtherBodyIndex,
        bFromSweep,
        SweepResult);
}
