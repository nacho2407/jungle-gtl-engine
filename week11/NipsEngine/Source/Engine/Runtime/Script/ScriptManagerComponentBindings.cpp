#include "Runtime/Script/ScriptManager.h"

#include "Asset/StaticMesh.h"
#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"
#include "Component/DecalComponent.h"
#include "Component/MeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SoundComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Runtime/Script/ScriptComponent.h"
#include "Runtime/Script/ScriptUtils.h"

namespace
{
    AActor* LuaGetComponentActor(UActorComponent& Component)
    {
        return Component.GetOwner();
    }
}

void FScriptManager::BindComponentTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorComponent, "ActorComponent", UObject)
    LUA_METHOD(GetOwner, GetOwner);
    LUA_SET(GetActor, &LuaGetComponentActor);
    LUA_METHOD(IsActive, IsActive);
    LUA_METHOD(SetActive, SetActive);
    LUA_METHOD(IsAutoActivate, IsAutoActivate);
    LUA_METHOD(SetAutoActivate, SetAutoActivate);
    LUA_METHOD(IsComponentTickEnabled, IsComponentTickEnabled);
    LUA_METHOD(SetComponentTickEnabled, SetComponentTickEnabled);
    LUA_METHOD(IsEditorOnly, IsEditorOnly);
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
    LUA_RO_PROPERTY(Forward, GetForwardVector);
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
