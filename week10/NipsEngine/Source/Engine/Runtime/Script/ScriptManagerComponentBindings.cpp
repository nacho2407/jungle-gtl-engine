#include "Runtime/Script/ScriptManager.h"

#include "Asset/StaticMesh.h"
#include "Component/ActorComponent.h"
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
    LUA_RW_PROPERTY(Sound, GetSound, SetSound);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UScriptComponent, "ScriptComponent", UActorComponent, UObject)
    LUA_METHOD(SetScriptName, SetScriptName);
    LUA_METHOD(GetScriptName, GetScriptName);
    LUA_METHOD(LoadScript, LoadScript);
    LUA_METHOD(HotReloadScript, HotReloadScript);
    LUA_METHOD(ClearScript, ClearScript);
    LUA_RW_PROPERTY(ScriptName, GetScriptName, SetScriptName);
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
