// [UHT generated master source - do not edit]
#include "ThirdParty/sol/sol.hpp"

#include "AActor.gen.cpp"
#include "ActorComponent.gen.cpp"
#include "ActorSequence.gen.cpp"
#include "ActorSequenceComponent.gen.cpp"
#include "AmbientLightComponent.gen.cpp"
#include "BillboardComponent.gen.cpp"
#include "BoxComponent.gen.cpp"
#include "CameraComponent.gen.cpp"
#include "CameraModifier.gen.cpp"
#include "CameraModifier_CameraShake.gen.cpp"
#include "CameraShakeBase.gen.cpp"
#include "CapsuleComponent.gen.cpp"
#include "Color.gen.cpp"
#include "CurveFloatAsset.gen.cpp"
#include "DecalComponent.gen.cpp"
#include "DefaultPawn.gen.cpp"
#include "DirectionalLightComponent.gen.cpp"
#include "EditorEngine.gen.cpp"
#include "Engine.gen.cpp"
#include "FireballComponent.gen.cpp"
#include "GameEngine.gen.cpp"
#include "GameModeBase.gen.cpp"
#include "GizmoComponent.gen.cpp"
#include "HeightFogComponent.gen.cpp"
#include "InterpToMovementComponent.gen.cpp"
#include "Level.gen.cpp"
#include "LightComponent.gen.cpp"
#include "LightComponentBase.gen.cpp"
#include "LineBatchComponent.gen.cpp"
#include "Material.gen.cpp"
#include "MeshComponent.gen.cpp"
#include "MovementComponent.gen.cpp"
#include "Pawn.gen.cpp"
#include "PlayerCameraManager.gen.cpp"
#include "PlayerController.gen.cpp"
#include "PointLightComponent.gen.cpp"
#include "PrimitiveActors.gen.cpp"
#include "PrimitiveComponent.gen.cpp"
#include "ProceduralMeshComponent.gen.cpp"
#include "ProjectileMovementComponent.gen.cpp"
#include "PursuitMovementComponent.gen.cpp"
#include "RotatingMovementComponent.gen.cpp"
#include "SceneComponent.gen.cpp"
#include "ScriptComponent.gen.cpp"
#include "SequenceCameraShakePattern.gen.cpp"
#include "ShadowTypes.gen.cpp"
#include "ShapeComponent.gen.cpp"
#include "SinusoidalCameraShakePattern.gen.cpp"
#include "SkeletalMesh.gen.cpp"
#include "SkeletalMeshComponent.gen.cpp"
#include "SkinnedMeshComponent.gen.cpp"
#include "SoundComponent.gen.cpp"
#include "SphereComponent.gen.cpp"
#include "SpotlightComponent.gen.cpp"
#include "SpringArmComponent.gen.cpp"
#include "StaticMesh.gen.cpp"
#include "StaticMeshComponent.gen.cpp"
#include "SubUVComponent.gen.cpp"
#include "TextRenderComponent.gen.cpp"
#include "Texture.gen.cpp"
#include "Vector.gen.cpp"
#include "Vector2.gen.cpp"
#include "Vector4.gen.cpp"
#include "World.gen.cpp"

void BindGeneratedLuaCasts(sol::state& Lua)
{
    sol::object ActorComponentObject = Lua["ActorComponent"];
    if (!ActorComponentObject.valid() || ActorComponentObject == sol::nil || ActorComponentObject.get_type() != sol::type::table)
    {
        return;
    }
    sol::table ActorComponentType = ActorComponentObject.as<sol::table>();

    ActorComponentType["AsActorSequenceComponent"] =
        [](UActorComponent& Self) -> UActorSequenceComponent*
        {
            return Cast<UActorSequenceComponent>(&Self);
        };
    ActorComponentType["AsBillboardComponent"] =
        [](UActorComponent& Self) -> UBillboardComponent*
        {
            return Cast<UBillboardComponent>(&Self);
        };
    ActorComponentType["AsBoxComponent"] =
        [](UActorComponent& Self) -> UBoxComponent*
        {
            return Cast<UBoxComponent>(&Self);
        };
    ActorComponentType["AsCameraComponent"] =
        [](UActorComponent& Self) -> UCameraComponent*
        {
            return Cast<UCameraComponent>(&Self);
        };
    ActorComponentType["AsCapsuleComponent"] =
        [](UActorComponent& Self) -> UCapsuleComponent*
        {
            return Cast<UCapsuleComponent>(&Self);
        };
    ActorComponentType["AsDecalComponent"] =
        [](UActorComponent& Self) -> UDecalComponent*
        {
            return Cast<UDecalComponent>(&Self);
        };
    ActorComponentType["AsFireballComponent"] =
        [](UActorComponent& Self) -> UFireballComponent*
        {
            return Cast<UFireballComponent>(&Self);
        };
    ActorComponentType["AsGizmoComponent"] =
        [](UActorComponent& Self) -> UGizmoComponent*
        {
            return Cast<UGizmoComponent>(&Self);
        };
    ActorComponentType["AsHeightFogComponent"] =
        [](UActorComponent& Self) -> UHeightFogComponent*
        {
            return Cast<UHeightFogComponent>(&Self);
        };
    ActorComponentType["AsLineBatchComponent"] =
        [](UActorComponent& Self) -> ULineBatchComponent*
        {
            return Cast<ULineBatchComponent>(&Self);
        };
    ActorComponentType["AsMeshComponent"] =
        [](UActorComponent& Self) -> UMeshComponent*
        {
            return Cast<UMeshComponent>(&Self);
        };
    ActorComponentType["AsInterpToMovementComponent"] =
        [](UActorComponent& Self) -> UInterpToMovementComponent*
        {
            return Cast<UInterpToMovementComponent>(&Self);
        };
    ActorComponentType["AsMovementComponent"] =
        [](UActorComponent& Self) -> UMovementComponent*
        {
            return Cast<UMovementComponent>(&Self);
        };
    ActorComponentType["AsProjectileMovementComponent"] =
        [](UActorComponent& Self) -> UProjectileMovementComponent*
        {
            return Cast<UProjectileMovementComponent>(&Self);
        };
    ActorComponentType["AsPursuitMovementComponent"] =
        [](UActorComponent& Self) -> UPursuitMovementComponent*
        {
            return Cast<UPursuitMovementComponent>(&Self);
        };
    ActorComponentType["AsRotatingMovementComponent"] =
        [](UActorComponent& Self) -> URotatingMovementComponent*
        {
            return Cast<URotatingMovementComponent>(&Self);
        };
    ActorComponentType["AsAmbientLightComponent"] =
        [](UActorComponent& Self) -> UAmbientLightComponent*
        {
            return Cast<UAmbientLightComponent>(&Self);
        };
    ActorComponentType["AsDirectionalLightComponent"] =
        [](UActorComponent& Self) -> UDirectionalLightComponent*
        {
            return Cast<UDirectionalLightComponent>(&Self);
        };
    ActorComponentType["AsLightComponent"] =
        [](UActorComponent& Self) -> ULightComponent*
        {
            return Cast<ULightComponent>(&Self);
        };
    ActorComponentType["AsLightComponentBase"] =
        [](UActorComponent& Self) -> ULightComponentBase*
        {
            return Cast<ULightComponentBase>(&Self);
        };
    ActorComponentType["AsPointLightComponent"] =
        [](UActorComponent& Self) -> UPointLightComponent*
        {
            return Cast<UPointLightComponent>(&Self);
        };
    ActorComponentType["AsSpotlightComponent"] =
        [](UActorComponent& Self) -> USpotlightComponent*
        {
            return Cast<USpotlightComponent>(&Self);
        };
    ActorComponentType["AsPrimitiveComponent"] =
        [](UActorComponent& Self) -> UPrimitiveComponent*
        {
            return Cast<UPrimitiveComponent>(&Self);
        };
    ActorComponentType["AsProceduralMeshComponent"] =
        [](UActorComponent& Self) -> UProceduralMeshComponent*
        {
            return Cast<UProceduralMeshComponent>(&Self);
        };
    ActorComponentType["AsSceneComponent"] =
        [](UActorComponent& Self) -> USceneComponent*
        {
            return Cast<USceneComponent>(&Self);
        };
    ActorComponentType["AsShapeComponent"] =
        [](UActorComponent& Self) -> UShapeComponent*
        {
            return Cast<UShapeComponent>(&Self);
        };
    ActorComponentType["AsSkeletalMeshComponent"] =
        [](UActorComponent& Self) -> USkeletalMeshComponent*
        {
            return Cast<USkeletalMeshComponent>(&Self);
        };
    ActorComponentType["AsSkinnedMeshComponent"] =
        [](UActorComponent& Self) -> USkinnedMeshComponent*
        {
            return Cast<USkinnedMeshComponent>(&Self);
        };
    ActorComponentType["AsSoundComponent"] =
        [](UActorComponent& Self) -> USoundComponent*
        {
            return Cast<USoundComponent>(&Self);
        };
    ActorComponentType["AsSphereComponent"] =
        [](UActorComponent& Self) -> USphereComponent*
        {
            return Cast<USphereComponent>(&Self);
        };
    ActorComponentType["AsSpringArmComponent"] =
        [](UActorComponent& Self) -> USpringArmComponent*
        {
            return Cast<USpringArmComponent>(&Self);
        };
    ActorComponentType["AsStaticMeshComponent"] =
        [](UActorComponent& Self) -> UStaticMeshComponent*
        {
            return Cast<UStaticMeshComponent>(&Self);
        };
    ActorComponentType["AsSubUVComponent"] =
        [](UActorComponent& Self) -> USubUVComponent*
        {
            return Cast<USubUVComponent>(&Self);
        };
    ActorComponentType["AsTextRenderComponent"] =
        [](UActorComponent& Self) -> UTextRenderComponent*
        {
            return Cast<UTextRenderComponent>(&Self);
        };
    ActorComponentType["AsMainSceneDestructibleComponent"] =
        [](UActorComponent& Self) -> UMainSceneDestructibleComponent*
        {
            return Cast<UMainSceneDestructibleComponent>(&Self);
        };
    ActorComponentType["AsScriptComponent"] =
        [](UActorComponent& Self) -> UScriptComponent*
        {
            return Cast<UScriptComponent>(&Self);
        };
}

void BindGeneratedLuaFunctions(sol::state& Lua)
{
    BindGeneratedLuaCasts(Lua);
    BindLua_USubUVComponent(Lua);
}
