#include "LuaWorldProxy.h"

#include "CameraManage/APlayerCameraManager.h"
#include "Component/CameraComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Core/RayTypes.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/LuaScriptActor.h"
#include "GameFramework/SubUVActor.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Physics/CollisionManager.h"

#include <cmath>

#include "CameraManage/APlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
constexpr float kLuaBlockedMoveStepLength = 0.05f;

FString NormalizeLuaActorClassName(const FString& Name)
{
    if (Name == "Actor")
        return "AActor";
    if (Name == "StaticMeshActor")
        return "AStaticMeshActor";
    if (Name == "PointLightActor")
        return "APointLightActor";
    if (Name == "SpotLightActor")
        return "ASpotLightActor";
    if (Name == "DirectionalLightActor")
        return "ADirectionalLightActor";
    if (Name == "HeightFogActor")
        return "AHeightFogActor";
    if (Name == "LuaScriptActor")
        return "ALuaScriptActor";
    if (Name == "DecalActor")
        return "ADecalActor";
    if (Name == "SubUVActor")
        return "ASubUVActor";

    return Name;
}

bool IsLuaSpawnableActorClassName(const FString& ClassName)
{
    return ClassName == "AActor"
		|| ClassName == "AStaticMeshActor"
		|| ClassName == "APointLightActor"
		|| ClassName == "ASpotLightActor" 
		|| ClassName == "ADirectionalLightActor" 
		|| ClassName == "AHeightFogActor" 
		|| ClassName == "ALuaScriptActor" 
		|| ClassName == "ADecalActor"
        || ClassName == "ASubUVActor";
}

APlayerCameraManager* GetPlayerCameraManager(UWorld* World)
{
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    if (!PlayerController)
    {
        return nullptr;
    }

    return PlayerController->GetCameraManager();
}

UCameraComponent* GetLuaControlledCamera(UWorld* World)
{
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetCameraManager() : nullptr;
    if (CameraManager)
    {
        if (UCameraComponent* POVCamera = CameraManager->GetViewTarget().POVCamera)
        {
            return POVCamera;
        }
    }

    return World ? World->GetActiveCamera() : nullptr;
}

bool GetLuaCameraViewInfo(UWorld* World, FCameraViewInfo& OutCameraViewInfo)
{
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetCameraManager() : nullptr;
    if (PlayerController && PlayerController->GetPossessedActor() && CameraManager)
    {
        OutCameraViewInfo = CameraManager->GetCameraViewInfoCache();
        return true;
    }

    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
    if (!Camera)
    {
        return false;
    }

    OutCameraViewInfo.Location = Camera->GetWorldLocation();
    OutCameraViewInfo.Rotation = Camera->GetWorldMatrix().ToRotator();
    OutCameraViewInfo.CameraState = Camera->GetCameraState();
    OutCameraViewInfo.ScreenEffects = CameraManager ? CameraManager->GetCameraViewInfoCache().ScreenEffects : FCameraScreenEffectInfo{};
    return true;
}

FMatrix MakeLuaCameraViewMatrix(const FCameraViewInfo& CameraViewInfo)
{
    const FVector F = CameraViewInfo.Rotation.GetForwardVector();
    const FVector R = CameraViewInfo.Rotation.GetRightVector();
    const FVector U = CameraViewInfo.Rotation.GetUpVector();
    const FVector E = CameraViewInfo.Location;

    return FMatrix(
        R.X, U.X, F.X, 0,
        R.Y, U.Y, F.Y, 0,
        R.Z, U.Z, F.Z, 0,
        -E.Dot(R), -E.Dot(U), -E.Dot(F), 1);
}

FMatrix MakeLuaCameraProjectionMatrix(const FCameraState& CameraState)
{
    if (!CameraState.bIsOrthogonal)
    {
        return FMatrix::MakePerspective(CameraState.FOV, CameraState.AspectRatio, CameraState.NearZ, CameraState.FarZ);
    }

    const float Width = CameraState.OrthoWidth;
    const float Height = Width / CameraState.AspectRatio;
    return FMatrix::MakeOrthographic(Width, Height, CameraState.NearZ, CameraState.FarZ);
}

} // namespace

void FLuaWorldProxy::SetWorld(UWorld* InWorld)
{
    WorldUUID = InWorld ? InWorld->GetUUID() : 0;
}

UWorld* FLuaWorldProxy::ResolveWorld() const
{
    if (WorldUUID == 0)
    {
        return nullptr;
    }

    return Cast<UWorld>(UObjectManager::Get().FindByUUID(WorldUUID));
}

bool FLuaWorldProxy::IsValid() const
{
    return ResolveWorld() != nullptr;
}

FLuaGameObjectProxy FLuaWorldProxy::SpawnActor(const FString& ActorClassName)
{
    UWorld* World = ResolveWorld();
    if (!World)
    {
        return FLuaGameObjectProxy();
    }

    const FString NormalizedName = NormalizeLuaActorClassName(ActorClassName);
    if (!IsLuaSpawnableActorClassName(NormalizedName))
    {
        UE_LOG(Lua, Warning,
               "Lua tried to spawn disallowed actor class: %s",
               ActorClassName.c_str());
        return FLuaGameObjectProxy();
    }

    AActor* Actor = World->SpawnActorByClassName(NormalizedName);
    if (!Actor)
    {
        UE_LOG(Lua, Warning,
               "Lua failed to spawn actor class: %s",
               NormalizedName.c_str());
        return FLuaGameObjectProxy();
    }

    if (ALuaScriptActor* LuaScriptActor = Cast<ALuaScriptActor>(Actor))
    {
        LuaScriptActor->InitDefaultComponents();
    }
    else if (ASubUVActor* SubUVActor = Cast<ASubUVActor>(Actor))
    {
        SubUVActor->InitDefaultComponents();
    }

    return FLuaGameObjectProxy(Actor);
}

bool FLuaWorldProxy::DestroyActor(const FLuaGameObjectProxy& ActorProxy)
{
    UWorld* World = ResolveWorld();
    AActor* Actor = ActorProxy.GetActor();
    if (!World || !Actor || Actor->GetWorld() != World)
    {
        return false;
    }

    World->QueueDestroyActor(Actor);
    return true;
}

FLuaGameObjectProxy FLuaWorldProxy::FindPlayer()
{
    UWorld* World = ResolveWorld();
    if (!World)
    {
        return FLuaGameObjectProxy();
    }

    TArray<AActor*> Actors = World->GetActors();
    for (AActor* Actor : Actors)
    {
        TArray<FName> Tags = Actor->GetTags();
        for (FName Tag : Tags)
        {
            if (Tag==FName("Player"))
            {
                return FLuaGameObjectProxy(Actor);
            }
        }
    }
    return FLuaGameObjectProxy();
}

FLuaGameObjectProxy FLuaWorldProxy::FindActorByTag(const FString& Tag)
{
    UWorld* World = ResolveWorld();
    if (!World || Tag.empty())
    {
        return FLuaGameObjectProxy();
    }

    TArray<AActor*> Actors = World->GetActors();
    for (AActor* Actor : Actors)
    {
        if (Actor && Actor->HasTag(Tag))
        {
            return FLuaGameObjectProxy(Actor);
        }
    }

    return FLuaGameObjectProxy();
}

TArray<FLuaGameObjectProxy> FLuaWorldProxy::FindActorsByTag(const FString& Tag)
{
    TArray<FLuaGameObjectProxy> Result;

    UWorld* World = ResolveWorld();
    if (!World || Tag.empty())
    {
        return Result;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->HasTag(Tag))
        {
            Result.emplace_back(Actor);
        }
    }

    return Result;
}

bool FLuaWorldProxy::SetCameraView(const FVector& Location,const FVector& Target, float FovDegrees)
{
    UWorld* World = ResolveWorld();
    UCameraComponent* Camera = GetLuaControlledCamera(World);
    if (!Camera) return false;
    Camera->SetWorldLocation(Location);
    Camera->LookAt(Target);
    Camera->SetFOV(FovDegrees * DEG_TO_RAD);
    return true;
}

bool FLuaWorldProxy::SetCameraViewWithBlend(const FVector& Location, const FVector& Target, float FovDegrees, float Duration, const FString& BlendType)
{
    UWorld* World = ResolveWorld();
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetCameraManager() : nullptr;

    if (!CameraManager)
    {
        return false;
    }

    CameraManager->SetCameraTransitionToView(Location, Target, FovDegrees, Duration, BlendType);
    return true;
}

bool FLuaWorldProxy::SetCameraViewImmediate(const FVector& Location, const FVector& Target, float FovDegrees)
{
    UWorld* World = ResolveWorld();
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetCameraManager() : nullptr;

    if (!CameraManager)
    {
        return false;
    }

    CameraManager->SetCameraViewImmediate(Location, Target, FovDegrees);
    return true;
}

bool FLuaWorldProxy::SetCameraTransitionToTarget(const FLuaGameObjectProxy& ActorProxy, float Duration, const FString& BlendType)
{
    UWorld* World = ResolveWorld();
    AActor* Actor = ActorProxy.GetActor();
    APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetCameraManager() : nullptr;

    if (!World || !Actor || Actor->GetWorld() != World || !CameraManager)
    {
        return false;
    }

    CameraManager->SetCameraTransitionToTarget(Actor, Duration, BlendType);
    return true;
}

bool FLuaWorldProxy::MoveActorWithBlock(const FLuaGameObjectProxy& ActorProxy, const FVector& Delta, const FString& BlockingTag)
{
    UWorld* World = ResolveWorld();
    AActor* Actor = ActorProxy.GetActor();
    FCollisionManager* CollisionManager = World ? World->GetCollisionManager() : nullptr;
    if (!World || !CollisionManager || !Actor || Actor->GetWorld() != World)
    {
        return false;
    }

    return CollisionManager->MoveActorWithBlock(Actor, Delta, BlockingTag, kLuaBlockedMoveStepLength);
}



FVector FLuaWorldProxy::GetActiveCameraLocation() const
{
    UWorld* World = ResolveWorld();
    UCameraComponent* Camera = GetLuaControlledCamera(World);
    return Camera ? Camera->GetWorldLocation() : FVector::ZeroVector;
}

FVector FLuaWorldProxy::GetActiveCameraForward() const
{
    UWorld* World = ResolveWorld();
    FCameraViewInfo CameraViewInfo;
    return GetLuaCameraViewInfo(World, CameraViewInfo) ? CameraViewInfo.Rotation.GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
}

FVector FLuaWorldProxy::GetActiveCameraRight() const
{
    UWorld* World = ResolveWorld();
    FCameraViewInfo CameraViewInfo;
    return GetLuaCameraViewInfo(World, CameraViewInfo) ? CameraViewInfo.Rotation.GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
}

FVector FLuaWorldProxy::GetActiveCameraUp() const
{
    UWorld* World = ResolveWorld();
    FCameraViewInfo CameraViewInfo;
    return GetLuaCameraViewInfo(World, CameraViewInfo) ? CameraViewInfo.Rotation.GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
}

FVector FLuaWorldProxy::GetMouseWorldPointOnPlane(float PlaneZ) const
{
    UWorld* World = ResolveWorld();
    FCameraViewInfo CameraViewInfo;
    FWindowsWindow* Window = GEngine ? GEngine->GetWindow() : nullptr;
    if (!GetLuaCameraViewInfo(World, CameraViewInfo) || !Window || Window->GetWidth() <= 0.0f || Window->GetHeight() <= 0.0f)
    {
        return FVector::ZeroVector;
    }

    POINT MouseClientPos = Window->ScreenToClientPoint(InputSystem::Get().GetSnapshot().MouseScreenPos);
    const float NdcX = (2.0f * static_cast<float>(MouseClientPos.x)) / Window->GetWidth() - 1.0f;
    const float NdcY = 1.0f - (2.0f * static_cast<float>(MouseClientPos.y)) / Window->GetHeight();

    const FVector NdcNear(NdcX, NdcY, 1.0f);
    const FVector NdcFar(NdcX, NdcY, 0.0f);

    const FMatrix ViewProjection = MakeLuaCameraViewMatrix(CameraViewInfo) * MakeLuaCameraProjectionMatrix(CameraViewInfo.CameraState);
    const FMatrix InverseViewProjection = ViewProjection.GetInverse();

    const FVector WorldNear = InverseViewProjection.TransformPositionWithW(NdcNear);
    const FVector WorldFar = InverseViewProjection.TransformPositionWithW(NdcFar);

    FRay MouseRay;
    MouseRay.Origin = WorldNear;

    FVector RayDirection = WorldFar - WorldNear;
    const float RayLength = std::sqrt(RayDirection.X * RayDirection.X + RayDirection.Y * RayDirection.Y + RayDirection.Z * RayDirection.Z);
    MouseRay.Direction = RayLength > 0.0001f ? RayDirection / RayLength : FVector(1.0f, 0.0f, 0.0f);

    if (std::abs(MouseRay.Direction.Z) < 0.0001f)
    {
        return MouseRay.Origin + MouseRay.Direction * 100.0f;
    }

    const float T = (PlaneZ - MouseRay.Origin.Z) / MouseRay.Direction.Z;
    if (T <= 0.0f)
    {
        return MouseRay.Origin + MouseRay.Direction * 100.0f;
    }

    return MouseRay.Origin + MouseRay.Direction * T;
}

void FLuaWorldProxy::PlayCameraShake(float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency)
{
    if (APlayerCameraManager* CameraManager = GetPlayerCameraManager(ResolveWorld()))
    {
        CameraManager->PlayCameraShake(Duration, LocationAmplitude, RotationAmplitude, Frequency);
    }
}

void FLuaWorldProxy::PlayCameraFade(float FromAmount, float ToAmount, float Duration, float R, float G, float B, float A)
{
    if (APlayerCameraManager* CameraManager = GetPlayerCameraManager(ResolveWorld()))
    {
        CameraManager->PlayCameraFade(FromAmount, ToAmount, Duration, FVector4(R, G, B, A));
    }
}

void FLuaWorldProxy::PlayCameraLetterBox(float FromAmount, float ToAmount, float Duration)
{
    if (APlayerCameraManager* CameraManager = GetPlayerCameraManager(ResolveWorld()))
    {
        CameraManager->PlayCameraLetterBox(FromAmount, ToAmount, Duration);
    }
}

void FLuaWorldProxy::PlayCameraGammaCorrection(float FromGamma, float ToGamma, float Duration)
{
    if (APlayerCameraManager* CameraManager = GetPlayerCameraManager(ResolveWorld()))
    {
        CameraManager->PlayCameraGammaCorrection(FromGamma, ToGamma, Duration);
    }
}

void FLuaWorldProxy::PlayCameraVignette(float FromIntensity, float ToIntensity, float Duration, float Radius, float Softness)
{
    if (APlayerCameraManager* CameraManager = GetPlayerCameraManager(ResolveWorld()))
    {
        CameraManager->PlayCameraVignette(FromIntensity, ToIntensity, Duration, Radius, Softness);
    }
}

bool FLuaWorldProxy::PlayCameraEffectAsset(const FString& AssetPath)
{
    if (APlayerCameraManager* CameraManager = GetPlayerCameraManager(ResolveWorld()))
    {
        return CameraManager->PlayCameraEffectAsset(AssetPath);
    }
    return false;
}

bool FLuaWorldProxy::RequestHitStop(float Duration) const
{
    UWorld* World = ResolveWorld();
    World->RequestHitStop(Duration);

    return true;
}

bool FLuaWorldProxy::RequestSlomo(float InTimeScale, float Duration) const
{
    UWorld* World = ResolveWorld();
    World->RequestSlomo(InTimeScale, Duration);

    return true;
}

float FLuaWorldProxy::GetUnscaledDeltatime() const
{
    UWorld* World = ResolveWorld();
    return World->GetUnScaledDeltatime();
}

float FLuaWorldProxy::GetTimeScale() const
{
    UWorld* World = ResolveWorld();
    return World->GetTimeScale();
}
