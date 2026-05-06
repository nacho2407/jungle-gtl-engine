#include "ScriptComponent.h"
#include "ScriptManager.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/ShakePattern/SequenceCameraShakePattern.h"
#include "Camera/ShakePattern/SinusoidalCameraShakePattern.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Asset/CurveFloatAsset.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Paths.h"
#include "Core/CollisionTypes.h"
#include "Core/ResourceManager.h"
#include <algorithm>
#include <filesystem>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

DEFINE_CLASS(UScriptComponent, UActorComponent)
REGISTER_FACTORY(UScriptComponent)

namespace
{
    sol::object GetLuaField(const sol::table& Table, const char* UpperName, const char* LowerName)
    {
        sol::object Value = Table[UpperName];
        if (Value.valid() && Value != sol::nil)
        {
            return Value;
        }
        return Table[LowerName];
    }

    float GetLuaFloatField(const sol::table& Table, const char* UpperName, const char* LowerName, float DefaultValue)
    {
        sol::object Value = GetLuaField(Table, UpperName, LowerName);
        return Value.valid() && Value.get_type() == sol::type::number
            ? Value.as<float>()
            : DefaultValue;
    }

    bool GetLuaBoolField(const sol::table& Table, const char* UpperName, const char* LowerName, bool DefaultValue)
    {
        sol::object Value = GetLuaField(Table, UpperName, LowerName);
        return Value.valid() && Value.get_type() == sol::type::boolean
            ? Value.as<bool>()
            : DefaultValue;
    }

    FString GetLuaStringField(const sol::table& Table, const char* UpperName, const char* LowerName, const FString& DefaultValue)
    {
        sol::object Value = GetLuaField(Table, UpperName, LowerName);
        return Value.valid() && Value.get_type() == sol::type::string
            ? Value.as<FString>()
            : DefaultValue;
    }

    ECurveTimeMappingMode GetLuaTimeMappingMode(const sol::table& Table)
    {
        const FString Mode = GetLuaStringField(Table, "TimeMappingMode", "timeMappingMode", "NormalizedTime");
        if (Mode == "CurveTime" || Mode == "Curve Time" || Mode == "curveTime")
        {
            return ECurveTimeMappingMode::CurveTime;
        }
        return ECurveTimeMappingMode::NormalizedTime;
    }

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

        sol::state_view Lua(Env.lua_state());
        sol::table Timeline = Lua.create_table();
        Timeline["New"] = [ScriptComponent]() -> FLuaTimeline*
        {
            return ScriptComponent->CreateTimeline();
        };
        Env["Timeline"] = Timeline;

        Env["WaitForSeconds"] = [](sol::this_state State, float Seconds)
        {
            sol::state_view Lua(State);
            sol::table Table = Lua.create_table();
            Table["type"] = "seconds";
            Table["value"] = Seconds;
            return Table;
        };

        Env["WaitForUnscaledSeconds"] = [](sol::this_state State, float Seconds)
        {
            sol::state_view Lua(State);
            sol::table Table = Lua.create_table();
            Table["type"] = "unscaled_seconds";
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

    float GetLuaNumberField(const sol::table& Table, const char* UpperName, const char* LowerName, int Index)
    {
        sol::object Value = Table[UpperName];
        if (Value.valid() && Value.get_type() == sol::type::number)
        {
            return Value.as<float>();
        }

        Value = Table[LowerName];
        if (Value.valid() && Value.get_type() == sol::type::number)
        {
            return Value.as<float>();
        }

        Value = Table[Index];
        if (Value.valid() && Value.get_type() == sol::type::number)
        {
            return Value.as<float>();
        }

        return 0.0f;
    }

    FVector GetLuaVectorDefault(const sol::table& PropTable)
    {
        sol::object DefaultObj = PropTable["Default"];
        if (!DefaultObj.valid() || DefaultObj == sol::nil)
        {
            return FVector();
        }

        if (DefaultObj.is<FVector>())
        {
            return DefaultObj.as<FVector>();
        }

        if (DefaultObj.get_type() == sol::type::table)
        {
            sol::table DefaultTable = DefaultObj.as<sol::table>();
            return FVector(
                GetLuaNumberField(DefaultTable, "X", "x", 1),
                GetLuaNumberField(DefaultTable, "Y", "y", 2),
                GetLuaNumberField(DefaultTable, "Z", "z", 3));
        }

        return FVector();
    }

    std::filesystem::file_time_type GetLuaScriptWriteTime(const FString& ScriptName)
    {
        FString ScriptPath;
        if (!FScriptManager::Get().ResolveScriptPath(ScriptName, ScriptPath))
        {
            return std::filesystem::file_time_type::min();
        }

        std::filesystem::path Path(FPaths::ToWide(ScriptPath));
        if (!std::filesystem::exists(Path))
        {
            return std::filesystem::file_time_type::min();
        }

        return std::filesystem::last_write_time(Path);
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
        OutDesc.Max = LuaProp.bHasMax ? LuaProp.Max : 1.0f;
        OutDesc.Speed = 0.1f;

        OutDesc.EnumNames = nullptr;
        OutDesc.EnumCount = 0;
        OutDesc.ExtraData = nullptr;

        return true;
    }

    void CopyCameraShakePatternBase(
        const UCameraShakePattern* Source,
        UCameraShakePattern* Target)
    {
        if (!Source || !Target)
        {
            return;
        }

        Target->Duration = Source->Duration;
        Target->BlendInTime = Source->BlendInTime;
        Target->BlendOutTime = Source->BlendOutTime;
    }

bool SetLuaTableProperty(sol::table& Table, const FLuaScriptProperty& Prop)
    {
        if (!Table.valid() || Prop.Name.empty())
        {
            return false;
        }

        const std::string Name = Prop.Name.c_str();
        switch (Prop.Type)
        {
        case EPropertyType::Int:
            Table[Name] = Prop.IntValue;
            return true;

        case EPropertyType::Float:
            Table[Name] = Prop.FloatValue;
            return true;

        case EPropertyType::Bool:
            Table[Name] = Prop.BoolValue;
            return true;

        case EPropertyType::String:
            Table[Name] = std::string(Prop.StringValue.c_str());
            return true;

        case EPropertyType::Vec3:
            Table[Name] = Prop.Vec3Value;
            return true;

        default:
            return false;
        }
    }

    UCameraShakePattern* CreateRuntimeCameraShakePattern(UCameraShakePattern* Source)
    {
        if (!Source)
        {
            return nullptr;
        }

        if (USinusoidalCameraShakePattern* Src = Cast<USinusoidalCameraShakePattern>(Source))
        {
            USinusoidalCameraShakePattern* Dst =
                UObjectManager::Get().CreateObject<USinusoidalCameraShakePattern>();

            CopyCameraShakePatternBase(Src, Dst);
            Dst->LocationAmplitude = Src->LocationAmplitude;
            Dst->LocationFrequency = Src->LocationFrequency;
            Dst->LocationPhase = Src->LocationPhase;
            Dst->RotationAmplitudeDeg = Src->RotationAmplitudeDeg;
            Dst->RotationFrequency = Src->RotationFrequency;
            Dst->RotationPhase = Src->RotationPhase;
            Dst->FOVAmplitude = Src->FOVAmplitude;
            Dst->FOVFrequency = Src->FOVFrequency;
            Dst->FOVPhase = Src->FOVPhase;
            return Dst;
        }

        if (USequenceCameraShakePattern* Src = Cast<USequenceCameraShakePattern>(Source))
        {
            USequenceCameraShakePattern* Dst =
                UObjectManager::Get().CreateObject<USequenceCameraShakePattern>();

            CopyCameraShakePatternBase(Src, Dst);
            Dst->Sequence = Src->Sequence;
            Dst->Curve = Src->Curve;
            Dst->PlayRate = Src->PlayRate;
            Dst->Scale = Src->Scale;
            Dst->RandomSegmentDuration = Src->RandomSegmentDuration;
            Dst->bRandomSegment = Src->bRandomSegment;
            Dst->bLoop = Src->bLoop;
            Dst->CurveAssetPath = Src->CurveAssetPath;
            Dst->LocationAmplitude = Src->LocationAmplitude;
            Dst->RotationAmplitudeDeg = Src->RotationAmplitudeDeg;
            Dst->FOVAmplitude = Src->FOVAmplitude;
            return Dst;
        }

        return Cast<UCameraShakePattern>(Source->Duplicate());
    }
}

void FLuaTimeline::Play()
{
    Player.Play();
}

void FLuaTimeline::Pause()
{
    Player.Pause();
}

void FLuaTimeline::Stop()
{
    Player.Stop();
}

void FLuaTimeline::Tick(float DeltaTime)
{
    Player.Tick(DeltaTime);
}

void FLuaTimeline::SetPlayRate(float InPlayRate)
{
    Player.SetPlayRate(InPlayRate);
}

void FLuaTimeline::SetLoop(bool bInLoop)
{
    Player.SetLoop(bInLoop);
}

float FLuaTimeline::GetCurrentTime() const
{
    return Player.GetCurrentTime();
}

void FLuaTimeline::SetCurrentTime(float InCurrentTime)
{
    Player.SetCurrentTime(InCurrentTime);
}

void FLuaTimeline::AddFloatTrack(
    const FString& TrackName,
    const sol::table& PlaybackDesc,
    sol::function OnUpdate)
{
    if (!OnUpdate.valid())
    {
        return;
    }

    const FCurvePlaybackDesc Playback = MakePlaybackDesc(PlaybackDesc);
    Player.AddFloatTrack(
        TrackName,
        Playback,
        [OnUpdate](float Value) mutable
        {
            sol::protected_function Callback = OnUpdate;
            sol::protected_function_result Result = Callback(Value);
            if (!Result.valid())
            {
                sol::error Error = Result;
                UE_LOG_ERROR("[LuaTimeline] callback failed: %s", Error.what());
            }
        });
}

void FLuaTimeline::ClearTracks()
{
    Player.ClearTracks();
}

FCurvePlaybackDesc FLuaTimeline::MakePlaybackDesc(const sol::table& PlaybackDesc) const
{
    FCurvePlaybackDesc Desc;
    Desc.StartTime = GetLuaFloatField(PlaybackDesc, "StartTime", "startTime", 0.0f);
    Desc.Duration = GetLuaFloatField(PlaybackDesc, "Duration", "duration", 1.0f);
    Desc.PlayRate = GetLuaFloatField(PlaybackDesc, "PlayRate", "playRate", 1.0f);
    Desc.bLoop = GetLuaBoolField(PlaybackDesc, "Loop", "loop", false);
    Desc.TimeMappingMode = GetLuaTimeMappingMode(PlaybackDesc);

    sol::object CurveObj = GetLuaField(PlaybackDesc, "Curve", "curve");
    if (CurveObj.valid() && CurveObj != sol::nil && CurveObj.is<UCurveFloatAsset*>())
    {
        Desc.Curve = CurveObj.as<UCurveFloatAsset*>();
    }

    Desc.CurveAssetPath = GetLuaStringField(PlaybackDesc, "CurveAssetPath", "curveAssetPath", "");
    if (Desc.CurveAssetPath.empty())
    {
        Desc.CurveAssetPath = GetLuaStringField(PlaybackDesc, "Path", "path", "");
    }

    if (!Desc.Curve && !Desc.CurveAssetPath.empty())
    {
        Desc.Curve = FResourceManager::Get().LoadCurve(Desc.CurveAssetPath);
    }

    return Desc;
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

    if (Ar.IsSaving() && !ScriptName.empty() && !bLuaPropertiesScanned)
    {
        ReloadLuaProperties();
    }

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

        const FString Prefix = "LuaProperty_" + std::to_string(i) + "_";

        Ar << (Prefix + "Name").c_str() << Prop.Name;
        Ar << (Prefix + "TypeName").c_str() << Prop.TypeName;
        Ar << (Prefix + "Category").c_str() << Prop.Category;

        if (Ar.IsLoading())
        {
            Prop.Type = ParseLuaScriptPropertyType(Prop.TypeName);
        }

        switch (Prop.Type)
        {
        case EPropertyType::Int:
            Ar << (Prefix + "IntValue").c_str() << Prop.IntValue;
            break;

        case EPropertyType::Float:
            Ar << (Prefix + "FloatValue").c_str() << Prop.FloatValue;
            break;

        case EPropertyType::Bool:
            Ar << (Prefix + "BoolValue").c_str() << Prop.BoolValue;
            break;

        case EPropertyType::String:
            Ar << (Prefix + "StringValue").c_str() << Prop.StringValue;
            break;

        case EPropertyType::Vec3:
            Ar << (Prefix + "Vec3Value").c_str() << Prop.Vec3Value;
            break;

        default:
            break;
        }

        Ar << (Prefix + "bHasMin").c_str() << Prop.bHasMin;
        Ar << (Prefix + "bHasMax").c_str() << Prop.bHasMax;
        Ar << (Prefix + "Min").c_str() << Prop.Min;
        Ar << (Prefix + "Max").c_str() << Prop.Max;
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
        if (bScriptLoaded)
        {
            HotReloadScript();
        }
        else
        {
            bLuaPropertiesScanned = false;
            ReloadLuaProperties();
        }
        return;
    }

    UnregisterScript();

    ScriptName = InScriptName;

    ClearLoadedState();
    bLuaPropertiesScanned = false;

    if (!ScriptName.empty())
    {
        ReloadLuaProperties();
    }
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (!PropertyName)
    {
        return;
    }

    if (FString(PropertyName) != "ScriptName")
    {
        ApplyLuaPropertyToInstance(PropertyName);
        return;
    }

    // 1. 기존 등록 해제
    UnregisterScript();

    // 2. 경로 문자열 정리
    // 여기서 ScriptName이 절대경로/상대경로로 들어올 수 있으면
    // 반드시 FScriptManager 기준 Script Reference로 변환해야 한다.
    FString NewScriptName = ScriptName;

    // 예시:
    // NewScriptName = FScriptManager::Get().MakeScriptReferenceFromPathOrName(ScriptName);
    // 네 코드에 MakeScriptReferenceFromPath() 같은 함수가 이미 있다면 그걸 써야 함.

    ScriptName = NewScriptName;

    // 3. Lua 상태 초기화
    ClearLoadedState();
    bLuaPropertiesScanned = false;

    // 4. 프로퍼티 재스캔
    if (!ScriptName.empty())
    {
        ReloadLuaProperties();
    }
}

bool UScriptComponent::ApplyLuaPropertyToInstance(const char* PropertyName)
{
    if (!PropertyName || !ScriptInstance.valid())
    {
        return false;
    }

    for (const FLuaScriptProperty& Prop : LuaProperties)
    {
        if (Prop.Name == PropertyName)
        {
            return SetLuaTableProperty(ScriptInstance, Prop);
        }
    }

    return false;
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
    RegisteredScriptName = ScriptName;
    bScriptRegistered = true;

    return true;
}

void UScriptComponent::UnregisterScript()
{
    if (!bScriptRegistered)
    {
        return;
    }

	if (!RegisteredScriptName.empty())
    {
        FScriptManager::Get().UnregisterScriptComponents(RegisteredScriptName, this);
    }
    else
    {
        FScriptManager::Get().UnregisterScriptComponentAll(this);
    }

    bScriptRegistered = false;
    RegisteredScriptName.clear();
}

void UScriptComponent::ClearScript()
{
    UnregisterScript();

    ScriptName.clear();
    RegisteredScriptName.clear();

    ClearLoadedState();
    LuaProperties.clear();
    bLuaPropertiesScanned = false;
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

    ReloadLuaProperties();

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

FLuaTimeline* UScriptComponent::CreateTimeline()
{
    FLuaTimeline* Timeline = new FLuaTimeline();
    LuaTimelines.push_back(Timeline);
    return Timeline;
}

void UScriptComponent::ClearLuaTimelines()
{
    for (FLuaTimeline* Timeline : LuaTimelines)
    {
        delete Timeline;
    }
    LuaTimelines.clear();
}

USequenceCameraShakePattern* UScriptComponent::CreateSequenceCameraShakePattern()
{
    USequenceCameraShakePattern* Pattern =
        UObjectManager::Get().CreateObject<USequenceCameraShakePattern>();
    CreatedCameraShakePatterns.push_back(Pattern);
    return Pattern;
}

USinusoidalCameraShakePattern* UScriptComponent::CreateSinusoidalCameraShakePattern()
{
    USinusoidalCameraShakePattern* Pattern =
        UObjectManager::Get().CreateObject<USinusoidalCameraShakePattern>();
    CreatedCameraShakePatterns.push_back(Pattern);
    return Pattern;
}

UCameraShakeBase* UScriptComponent::StartCameraShakePattern(
    UCameraShakePattern* Pattern,
    float Scale,
    float DurationOverride)
{
    if (!Pattern)
    {
        return nullptr;
    }

    UCameraShakePattern* RuntimePattern =
        CreateRuntimeCameraShakePattern(Pattern);
    if (!RuntimePattern)
    {
        return nullptr;
    }

    APlayerController* PC = GEngine ? GEngine->GetPrimaryPlayerController() : nullptr;
    APlayerCameraManager* CameraManager = PC ? PC->GetPlayerCameraManager() : nullptr;
    if (!CameraManager)
    {
        UObjectManager::Get().DestroyObject(RuntimePattern);
        return nullptr;
    }

    return CameraManager->StartCameraShake(RuntimePattern, Scale, DurationOverride);
}

void UScriptComponent::StopCameraShake(UCameraShakeBase* Shake, bool bImmediately)
{
    if (!Shake)
    {
        return;
    }

    APlayerCameraManager* CameraManager = Shake->GetCameraManager();
    if (!CameraManager)
    {
        APlayerController* PC = GEngine ? GEngine->GetPrimaryPlayerController() : nullptr;
        CameraManager = PC ? PC->GetPlayerCameraManager() : nullptr;
    }

    if (!CameraManager)
    {
        Shake->StopShake(bImmediately);
        return;
    }

    UCameraModifier_CameraShake* Modifier =
        CameraManager->FindCameraModifier<UCameraModifier_CameraShake>();
    if (Modifier)
    {
        Modifier->StopCameraShake(Shake, bImmediately);
        return;
    }

    Shake->StopShake(bImmediately);
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

    for (FLuaTimeline* Timeline : LuaTimelines)
    {
        if (Timeline)
        {
            Timeline->Tick(DeltaTime);
        }
    }

    float UnscaledDeltaTime = DeltaTime;
    if (AActor* OwnerActor = GetOwner())
    {
        if (UWorld* World = OwnerActor->GetFocusedWorld())
        {
            UnscaledDeltaTime = World->GetUnscaledDeltaTime();
        }
    }

    CoroutineScheduler.Tick(DeltaTime, UnscaledDeltaTime);
}

void UScriptComponent::EndPlay()
{
    UActorComponent::EndPlay();

	if (!ScriptEnv.valid())
	{
        CoroutineScheduler.StopAll();
        UnregisterScript();
        ClearLoadedState();
		return;
	}

    CallScriptFunction("EndPlay");
    CoroutineScheduler.StopAll();
    UnregisterScript();
    ClearLoadedState();
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

        // 기존 값을 보존해야 하는 경로(씬 로드/복제)에서는 같은 타입의 기존 값을 유지한다.
        FLuaScriptProperty* OldProp = FindPropertyInArray(OldProperties, PropName);

        if (OldProp != nullptr && OldProp->Type == PropType)
        {
            NewProp.IntValue = OldProp->IntValue;
            NewProp.FloatValue = OldProp->FloatValue;
            NewProp.BoolValue = OldProp->BoolValue;
            NewProp.StringValue = OldProp->StringValue;
            NewProp.Vec3Value = OldProp->Vec3Value;
        }
        else
        {
            // Lua 파일 변경/ScriptName 변경 시에는 Lua의 Default를 다시 반영한다.
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

            case EPropertyType::Vec3:
                NewProp.Vec3Value = GetLuaVectorDefault(PropTable);
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
            if (A.Category != B.Category)
            {
                return A.Category < B.Category;
            }
            if (A.TypeName != B.TypeName)
            {
                return A.TypeName < B.TypeName;
            }
            return A.Name < B.Name;
        });
}

sol::table UScriptComponent::MakeRuntimePropertyTable(sol::state& Lua)
{
    sol::table Table = Lua.create_table();

    for (const FLuaScriptProperty& Prop : LuaProperties)
    {
        SetLuaTableProperty(Table, Prop);
    }

    return Table;
}


void UScriptComponent::ClearLoadedState()
{
    bScriptLoaded = false;
    CoroutineScheduler.StopAll();
    ClearLuaTimelines();
    DestroyCreatedCameraShakePatterns();

    ScriptEnv = sol::environment{};
    ScriptClass = sol::table{};
    ScriptInstance = sol::table{};
}

void UScriptComponent::DestroyCreatedCameraShakePatterns()
{
    for (UCameraShakePattern* Pattern : CreatedCameraShakePatterns)
    {
        UObjectManager::Get().DestroyObject(Pattern);
    }

    CreatedCameraShakePatterns.clear();
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
