#include "Runtime/Script/ScriptManager.h"

#include "Asset/CurveFloatAsset.h"
#include "Asset/StaticMesh.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/ShakePattern/SequenceCameraShakePattern.h"
#include "Camera/ShakePattern/SinusoidalCameraShakePattern.h"
#include "Component/ActorComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/BoxComponent.h"
#include "Component/CameraComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/DecalComponent.h"
#include "Component/FireballComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/PursuitMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PostProcess/Light/LightComponent.h"
#include "Component/PostProcess/Light/LightComponentBase.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SoundComponent.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "GameFramework/AActor.h"
#include "Runtime/Script/ScriptComponent.h"
#include "Runtime/Script/ScriptUtils.h"

#include <algorithm>
#include <cctype>

namespace
{
    AActor* LuaGetComponentActor(UActorComponent& Component)
    {
        return Component.GetOwner();
    }

    sol::table ComponentTagsToLuaTable(sol::this_state State, UActorComponent& Component)
    {
        sol::state_view Lua(State);
        sol::table Tags = Lua.create_table();

        int32 Index = 1;
        for (const FString& Tag : Component.GetTags())
        {
            Tags[Index++] = Tag;
        }
        return Tags;
    }

    FString LuaGetStringField(
        const sol::table& Table,
        const char* LowerName,
        const char* UpperName,
        const char* FallbackName = nullptr)
    {
        FString Value = Table[LowerName].get_or<FString>("");
        if (!Value.empty())
        {
            return Value;
        }

        Value = Table[UpperName].get_or<FString>("");
        if (!Value.empty() || !FallbackName)
        {
            return Value;
        }

        return Table[FallbackName].get_or<FString>("");
    }

    FString LuaToLower(FString Value)
    {
        std::transform(
            Value.begin(),
            Value.end(),
            Value.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Value;
    }

    ECurveApplyMode LuaParseCurveApplyMode(const FString& Text)
    {
        const FString Lower = LuaToLower(Text);
        if (Lower.empty() || Lower == "absolute")
        {
            return ECurveApplyMode::Absolute;
        }
        if (Lower == "additive")
        {
            return ECurveApplyMode::Additive;
        }
        if (Lower == "multiply")
        {
            return ECurveApplyMode::Multiply;
        }

        UE_LOG_WARNING(
            "[ActorSequence] Unsupported applyMode '%s'. Use Absolute, Additive, or Multiply.",
            Text.c_str());
        return ECurveApplyMode::Absolute;
    }

    ECurveTimeMappingMode LuaParseCurveTimeMappingMode(const FString& Text)
    {
        const FString Lower = LuaToLower(Text);
        if (Lower == "curvetime" || Lower == "curve_time" || Lower == "curve time")
        {
            return ECurveTimeMappingMode::CurveTime;
        }
        return ECurveTimeMappingMode::NormalizedTime;
    }

    bool LuaAddActorSequenceFloatTrack(UActorSequenceComponent& Component, const sol::table& Desc)
    {
        FActorSequenceFloatTrackDesc TrackDesc;
        TrackDesc.TargetObjectName = LuaGetStringField(
            Desc,
            "targetObjectName",
            "TargetObjectName",
            "targetComponent");
        if (TrackDesc.TargetObjectName.empty())
        {
            TrackDesc.TargetObjectName = Desc["TargetComponent"].get_or<FString>("");
        }

        TrackDesc.TargetPropertyPath = LuaGetStringField(
            Desc,
            "targetPropertyPath",
            "TargetPropertyPath",
            "targetProperty");
        if (TrackDesc.TargetPropertyPath.empty())
        {
            TrackDesc.TargetPropertyPath = Desc["TargetProperty"].get_or<FString>("");
        }

        TrackDesc.CurveAssetPath = LuaGetStringField(
            Desc,
            "curveAssetPath",
            "CurveAssetPath",
            "curve");
        if (TrackDesc.CurveAssetPath.empty())
        {
            TrackDesc.CurveAssetPath = Desc["Curve"].get_or<FString>("");
        }

        TrackDesc.StartTime = Desc["startTime"].get_or(Desc["StartTime"].get_or(0.0f));
        TrackDesc.Duration = Desc["duration"].get_or(Desc["Duration"].get_or(1.0f));
        TrackDesc.PlayRate = Desc["playRate"].get_or(Desc["PlayRate"].get_or(1.0f));
        TrackDesc.bLoop = Desc["loop"].get_or(Desc["Loop"].get_or(false));
        TrackDesc.ApplyMode = LuaParseCurveApplyMode(
            Desc["applyMode"].get_or(Desc["ApplyMode"].get_or(FString("Absolute"))));
        TrackDesc.TimeMappingMode = LuaParseCurveTimeMappingMode(
            Desc["timeMappingMode"].get_or(Desc["TimeMappingMode"].get_or(FString("NormalizedTime"))));

        return Component.AddFloatTrack(TrackDesc);
    }
}

void FScriptManager::BindComponentTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorComponent, "ActorComponent", UObject)
    LUA_PROPERTY(TypeName, [](UActorComponent& Component) -> FString
                 { return Component.GetTypeInfo() && Component.GetTypeInfo()->name ? Component.GetTypeInfo()->name : ""; });
    LUA_METHOD(GetOwner, GetOwner);
    LUA_SET(GetActor, &LuaGetComponentActor);
    LUA_SET(AsSceneComponent, [](UActorComponent& Self) -> USceneComponent*
				 { return Cast<USceneComponent>(&Self); });
    LUA_SET(AsBoxComponent, [](UActorComponent& Self) -> UBoxComponent*
            { return Cast<UBoxComponent>(&Self); });
    LUA_SET(AsActorSequenceComponent, [](UActorComponent& Self) -> UActorSequenceComponent*
            { return Cast<UActorSequenceComponent>(&Self); });
    LUA_METHOD(IsActive, IsActive);
    LUA_METHOD(SetActive, SetActive);
    LUA_METHOD(IsAutoActivate, IsAutoActivate);
    LUA_METHOD(SetAutoActivate, SetAutoActivate);
    LUA_METHOD(IsComponentTickEnabled, IsComponentTickEnabled);
    LUA_METHOD(SetComponentTickEnabled, SetComponentTickEnabled);
    LUA_METHOD(IsEditorOnly, IsEditorOnly);
    LUA_METHOD(AddTag, AddTag);
    LUA_METHOD(RemoveTag, RemoveTag);
    LUA_METHOD(HasTag, HasTag);
    LUA_METHOD(ClearTags, ClearTags);
    LUA_SET(GetTags, &ComponentTagsToLuaTable);
    LUA_RO_PROPERTY(Owner, GetOwner);
    LUA_RO_PROPERTY(Actor, GetOwner);
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
    LUA_RW_PROPERTY(Rotation, GetRelativeRotation, SetRelativeRotation);
    LUA_RW_PROPERTY(Scale, GetRelativeScale, SetRelativeScale);
    LUA_RO_PROPERTY(Forward, GetForwardVector);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UMovementComponent, "MovementComponent", UActorComponent, UObject)
    LUA_METHOD(SetUpdatedComponent, SetUpdatedComponent);
    LUA_METHOD(GetUpdatedComponent, GetUpdatedComponent);
    LUA_SET(AddInputVector, [](UMovementComponent& Self, const FVector& WorldDirection, sol::optional<float> Scale)
            { Self.AddInputVector(WorldDirection, Scale.value_or(1.0f)); });
    LUA_METHOD(ConsumeInputVector, ConsumeInputVector);
    LUA_METHOD(GetMaxSpeed, GetMaxSpeed);
    LUA_METHOD(IsExceedingMaxSpeed, IsExceedingMaxSpeed);
    LUA_METHOD(StopMovementImmediately, StopMovementImmediately);
    LUA_METHOD(ConstrainDirectionToPlane, ConstrainDirectionToPlane);
    LUA_METHOD(ConstrainLocationToPlane, ConstrainLocationToPlane);
    LUA_RW_PROPERTY(Velocity, GetVelocity, SetVelocity);
    LUA_RW_PROPERTY(PendingInputVector, GetPendingInputVector, SetPendingInputVector);
    LUA_RW_PROPERTY(PlaneConstraintNormal, GetPlaneConstraintNormal, SetPlaneConstraintNormal);
    LUA_RO_PROPERTY(UpdatedComponent, GetUpdatedComponent);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UProjectileMovementComponent, "ProjectileMovementComponent", UMovementComponent, UActorComponent, UObject)
    LUA_RW_PROPERTY(InitialSpeed, GetInitialSpeed, SetInitialSpeed);
    LUA_RW_PROPERTY(MaxSpeed, GetMaxSpeed, SetMaxSpeed);
    LUA_RW_PROPERTY(GravityScale, GetGravityScale, SetGravityScale);
    LUA_RW_PROPERTY(RotationFollowsVelocity, GetRotationFollowsVelocity, SetRotationFollowsVelocity);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UPursuitMovementComponent, "PursuitMovementComponent", UMovementComponent, UActorComponent, UObject)
    LUA_METHOD(ClearTarget, ClearTarget);
    LUA_METHOD(IsInPursuit, IsInPursuit);
    LUA_RW_PROPERTY(FacingTargetDirection, IsFacingTargetDir, ShouldFaceTargetDir);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, URotatingMovementComponent, "RotatingMovementComponent", UMovementComponent, UActorComponent, UObject)
    LUA_RW_PROPERTY(RotationRate, GetRotationRate, SetRotationSpeed);
    LUA_RW_PROPERTY(PivotTranslation, GetPivotTranslation, SetPivotTranslation);
    LUA_RW_PROPERTY(RotationInLocalSpace, IsRotationInLocalSpace, SetRotationInLocalSpace);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UInterpToMovementComponent, "InterpToMovementComponent", UMovementComponent, UActorComponent, UObject)
    LUA_METHOD(AddControlPoint, AddControlPoint);
    LUA_METHOD(RemoveControlPoint, RemoveControlPoint);
    LUA_METHOD(SetControlPoint, SetControlPoint);
    LUA_RW_PROPERTY(Duration, GetInterpDuration, SetInterpDuration);
    LUA_RW_PROPERTY(FacingTargetDirection, IsFacingTargetDir, ShouldFaceTargetDir);
    LUA_RW_PROPERTY(AutoActivate, IsAutoActivating, ShouldAutoActivate);
    LUA_METHOD(Initiate, Initiate);
    LUA_METHOD(Reset, Reset);
    LUA_METHOD(ResetAndHalt, ResetAndHalt);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, ULightComponentBase, "LightComponentBase", USceneComponent, UActorComponent, UObject)
    LUA_FIELD(Intensity, Intensity);
    LUA_FIELD(CastShadows, bCastShadows);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, ULightComponent, "LightComponent", ULightComponentBase, USceneComponent, UActorComponent, UObject)
    LUA_FIELD(ShadowResolutionScale, ShadowResolutionScale);
    LUA_FIELD(ConstantBias, ConstantBias);
    LUA_FIELD(SlopeScaledBias, SlopeScaledBias);
    LUA_FIELD(ShadowSharpen, ShadowSharpen);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UAmbientLightComponent, "AmbientLightComponent", ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject)
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UDirectionalLightComponent, "DirectionalLightComponent", ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject)
    LUA_FIELD(CSMMaxDistance, CSMMaxDistance);
    LUA_FIELD(CSMPractialLambda, CSMPractialLambda);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UPointLightComponent, "PointLightComponent", ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject)
    LUA_FIELD(AttenuationRadius, AttenuationRadius);
    LUA_FIELD(LightFalloffExponent, LightFalloffExponent);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USpotlightComponent, "SpotlightComponent", UPointLightComponent, ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject)
    LUA_FIELD(InnerConeAngle, InnerConeAngle);
    LUA_FIELD(OuterConeAngle, OuterConeAngle);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USoundComponent, "SoundComponent", USceneComponent, UActorComponent, UObject)
    LUA_METHOD(Play, Play);
    LUA_METHOD(Stop, Stop);
    LUA_METHOD(IsPlaying, IsPlaying);
    LUA_METHOD(SetSound, SetSound);
    LUA_METHOD(GetSound, GetSound);
    LUA_METHOD(SetPlayOnBeginPlay, SetPlayOnBeginPlay);
    LUA_METHOD(IsPlayOnBeginPlay, IsPlayOnBeginPlay);
    LUA_METHOD(SetLoop, SetLoop);
    LUA_METHOD(IsLooping, IsLooping);
    LUA_METHOD(SetSpatialized, SetSpatialized);
    LUA_METHOD(IsSpatialized, IsSpatialized);
    LUA_METHOD(SetVolumeScale, SetVolumeScale);
    LUA_METHOD(GetVolumeScale, GetVolumeScale);
    LUA_METHOD(Set3DMinMaxDistance, Set3DMinMaxDistance);
    LUA_METHOD(Get3DMinDistance, Get3DMinDistance);
    LUA_METHOD(Get3DMaxDistance, Get3DMaxDistance);
    LUA_METHOD(Set3DAttenuation, Set3DAttenuation);
    LUA_METHOD(Get3DAttenuationModel, Get3DAttenuationModel);
    LUA_METHOD(Get3DRolloffFactor, Get3DRolloffFactor);
    LUA_RW_PROPERTY(Sound, GetSound, SetSound);
    LUA_RW_PROPERTY(Looping, IsLooping, SetLoop);
    LUA_RW_PROPERTY(Spatialized, IsSpatialized, SetSpatialized);
    LUA_RW_PROPERTY(VolumeScale, GetVolumeScale, SetVolumeScale);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UScriptComponent, "ScriptComponent", UActorComponent, UObject)
    LUA_METHOD(SetScriptName, SetScriptName);
    LUA_METHOD(GetScriptName, GetScriptName);
    LUA_METHOD(LoadScript, LoadScript);
    LUA_METHOD(HotReloadScript, HotReloadScript);
    LUA_METHOD(ClearScript, ClearScript);
    LUA_METHOD(CreateSequenceCameraShakePattern, CreateSequenceCameraShakePattern);
    LUA_METHOD(CreateSinusoidalCameraShakePattern, CreateSinusoidalCameraShakePattern);
    LUA_METHOD(StartCameraShakePattern, StartCameraShakePattern);
    LUA_METHOD(StopCameraShake, StopCameraShake);
    LUA_RW_PROPERTY(ScriptName, GetScriptName, SetScriptName);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UCameraShakeBase, "CameraShakeBase", UObject)
    LUA_METHOD(StopShake, StopShake);
    LUA_METHOD(IsFinished, IsFinished);
    LUA_METHOD(GetRootShakePattern, GetRootShakePattern);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UCameraShakePattern, "CameraShakePattern", UObject)
    LUA_FIELD(Duration, Duration);
    LUA_FIELD(BlendInTime, BlendInTime);
    LUA_FIELD(BlendOutTime, BlendOutTime);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USequenceCameraShakePattern, "SequenceCameraShakePattern", UCameraShakePattern, UObject)
    LUA_FIELD(PlayRate, PlayRate);
    LUA_FIELD(Scale, Scale);
    LUA_FIELD(RandomSegmentDuration, RandomSegmentDuration);
    LUA_FIELD(bRandomSegment, bRandomSegment);
    LUA_FIELD(bLoop, bLoop);
    LUA_FIELD(CurveAssetPath, CurveAssetPath);
    LUA_FIELD(LocationAmplitude, LocationAmplitude);
    LUA_FIELD(RotationAmplitudeDeg, RotationAmplitudeDeg);
    LUA_FIELD(FOVAmplitude, FOVAmplitude);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USinusoidalCameraShakePattern, "SinusoidalCameraShakePattern", UCameraShakePattern, UObject)
    LUA_FIELD(LocationAmplitude, LocationAmplitude);
    LUA_FIELD(LocationFrequency, LocationFrequency);
    LUA_FIELD(LocationPhase, LocationPhase);
    LUA_FIELD(RotationAmplitudeDeg, RotationAmplitudeDeg);
    LUA_FIELD(RotationFrequency, RotationFrequency);
    LUA_FIELD(RotationPhase, RotationPhase);
    LUA_FIELD(FOVAmplitude, FOVAmplitude);
    LUA_FIELD(FOVFrequency, FOVFrequency);
    LUA_FIELD(FOVPhase, FOVPhase);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorSequenceComponent, "ActorSequenceComponent", UActorComponent, UObject)
    LUA_METHOD(Play, Play);
    LUA_METHOD(Pause, Pause);
    LUA_METHOD(Stop, Stop);
    LUA_METHOD(GetSequence, GetSequence);
    LUA_METHOD(GetSequencePlayer, GetSequencePlayer);
    LUA_METHOD(IsAutoPlay, IsAutoPlay);
    LUA_METHOD(SetAutoPlay, SetAutoPlay);
    LUA_METHOD(IsLooping, IsLooping);
    LUA_METHOD(SetLooping, SetLooping);
    LUA_METHOD(GetPlayRate, GetPlayRate);
    LUA_METHOD(SetPlayRate, SetPlayRate);
    LUA_METHOD(ShouldPauseAtEnd, ShouldPauseAtEnd);
    LUA_METHOD(SetPauseAtEnd, SetPauseAtEnd);
    LUA_METHOD(GetStartOffsetSeconds, GetStartOffsetSeconds);
    LUA_METHOD(SetStartOffsetSeconds, SetStartOffsetSeconds);
    LUA_SET(AddFloatTrack, &LuaAddActorSequenceFloatTrack);
    LUA_END_TYPE();
}

void FScriptManager::BindStaticMeshTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UStaticMesh, "StaticMesh", UObject)
    LUA_METHOD(GetAssetPath, GetAssetPathFileName);
    LUA_METHOD(HasValidMesh, HasValidMeshData);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UMeshComponent, "MeshComponent", UPrimitiveComponent, USceneComponent, UActorComponent, UObject)
    LUA_METHOD(GetNumMaterials, GetNumMaterials);
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
    LUA_SET(SetVignette, [](UCameraComponent& Self, float Intensity, sol::optional<float> Radius, sol::optional<float> Smoothness,
                            sol::optional<float> R, sol::optional<float> G, sol::optional<float> B)
            { Self.SetVignette(
                Intensity,
                Radius.value_or(0.75f),
                Smoothness.value_or(0.35f),
                FColor(R.value_or(0.0f), G.value_or(0.0f), B.value_or(0.0f), 1.0f)); });
    LUA_METHOD(ClearVignette, ClearVignette);
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

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UShapeComponent, "ShapeComponent",
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UBoxComponent, "BoxComponent",
                                UShapeComponent,
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USphereComponent, "SphereComponent",
                                UShapeComponent,
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_METHOD(GetSphereRadius, GetSphereRadius);
    LUA_METHOD(GetScaledSphereRadius, GetScaledSphereRadius);
    LUA_RO_PROPERTY(SphereRadius, GetSphereRadius);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UCapsuleComponent, "CapsuleComponent",
                                UShapeComponent,
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_METHOD(GetCapsuleHalfHeight, GetCapsuleHalfHeight);
    LUA_METHOD(GetCapsuleRadius, GetCapsuleRadius);
    LUA_METHOD(GetScaledCapsuleHalfHeight, GetScaledCapsuleHalfHeight);
    LUA_METHOD(GetScaledCapsuleRadius, GetScaledCapsuleRadius);
    LUA_RO_PROPERTY(CapsuleHalfHeight, GetCapsuleHalfHeight);
    LUA_RO_PROPERTY(CapsuleRadius, GetCapsuleRadius);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UFireballComponent, "FireballComponent",
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_RW_PROPERTY(Intensity, GetIntensity, SetIntensity);
    LUA_RW_PROPERTY(Radius, GetRadius, SetRadius);
    LUA_RW_PROPERTY(RadiusFallOff, GetRadiusFallOff, SetRadiusFallOff);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UHeightFogComponent, "HeightFogComponent",
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_RW_PROPERTY(FogDensity, GetFogDensity, SetFogDensity);
    LUA_RW_PROPERTY(HeightFalloff, GetHeightFalloff, SetHeightFalloff);
    LUA_RW_PROPERTY(FogHeight, GetFogHeight, SetFogHeight);
    LUA_RW_PROPERTY(FogStartDistance, GetFogStartDistance, SetFogStartDistance);
    LUA_RW_PROPERTY(FogCutoffDistance, GetFogCutoffDistance, SetFogCutoffDistance);
    LUA_RW_PROPERTY(FogMaxOpacity, GetFogMaxOpacity, SetFogMaxOpacity);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, USubUVComponent, "SubUVComponent",
                                UBillboardComponent,
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_SET(SetParticle, [](USubUVComponent& Self, const FString& ParticleName)
            { Self.SetParticle(FName(ParticleName)); });
    LUA_METHOD(GetParticleName, GetParticleName);
    LUA_RW_PROPERTY(FrameIndex, GetFrameIndex, SetFrameIndex);
    LUA_METHOD(SetFrameRate, SetFrameRate);
    LUA_RW_PROPERTY(Loop, IsLoop, SetLoop);
    LUA_METHOD(IsFinished, IsFinished);
    LUA_METHOD(Play, Play);
    LUA_SET(SetSpriteSize, [](USubUVComponent& Self, float Width, float Height)
            { Self.SetSpriteSize(Width, Height); });
    LUA_METHOD(GetWidth, GetWidth);
    LUA_METHOD(GetHeight, GetHeight);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UTextRenderComponent, "TextRenderComponent",
                                UPrimitiveComponent,
                                USceneComponent,
                                UActorComponent,
                                UObject)
    LUA_RW_PROPERTY(Text, GetText, SetText);
    LUA_SET(SetFont, [](UTextRenderComponent& Self, const FString& FontName)
            { Self.SetFont(FName(FontName)); });
    LUA_METHOD(GetFontName, GetFontName);
    LUA_RW_PROPERTY(FontSize, GetFontSize, SetFontSize);
    LUA_SET(SetScreenPosition, [](UTextRenderComponent& Self, float X, float Y)
            { Self.SetScreenPosition(X, Y); });
    LUA_METHOD(GetScreenX, GetScreenX);
    LUA_METHOD(GetScreenY, GetScreenY);
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
