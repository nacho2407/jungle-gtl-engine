#include "LuaComponentProxy.h"

#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/LightComponentBase.h"
#include "Component/LuaScriptComponent.h"
#include "Component/PatrolAgentComponent.h"
#include "Component/PatrolPointComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjManager.h"
#include "Object/Object.h"
#include "Runtime/Engine.h"

FLuaComponentProxy::FLuaComponentProxy(UActorComponent* InComponent)
{
    SetComponent(InComponent);
}

void FLuaComponentProxy::SetComponent(UActorComponent* InComponent)
{
    ComponentUUID = InComponent ? InComponent->GetUUID() : 0;
}

UActorComponent* FLuaComponentProxy::ResolveComponent() const
{
    if (ComponentUUID == 0)
    {
        return nullptr;
    }

    return Cast<UActorComponent>(UObjectManager::Get().FindByUUID(ComponentUUID));
}

bool FLuaComponentProxy::IsValid() const
{
    return ResolveComponent() != nullptr;
}

FString FLuaComponentProxy::GetComponentClassName() const
{
    UActorComponent* Component = ResolveComponent();
    return Component && Component->GetClass() ? Component->GetClass()->GetName() : "";
}

FVector FLuaComponentProxy::GetForwardVector() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorForward() : FVector(1.0f, 0.0f, 0.0f);
}

FVector FLuaComponentProxy::GetRightVector() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorRightVector() : FVector(0.0f, 1.0f, 0.0f);
}

FVector FLuaComponentProxy::GetUpVector() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorUpVector() : FVector(0.0f, 0.0f, 1.0f);
}


USceneComponent* FLuaComponentProxy::ResolveSceneComponent() const
{
    return Cast<USceneComponent>(ResolveComponent());
}

UPrimitiveComponent* FLuaComponentProxy::ResolvePrimitiveComponent() const
{
    return Cast<UPrimitiveComponent>(ResolveComponent());
}

UStaticMeshComponent* FLuaComponentProxy::ResolveStaticMeshComponent() const
{
    return Cast<UStaticMeshComponent>(ResolveComponent());
}

USubUVComponent* FLuaComponentProxy::ResolveSubUVComponent() const
{
    return Cast<USubUVComponent>(ResolveComponent());
}

AActor* FLuaComponentProxy::ResolveActor() const
{
    UActorComponent* Component = ResolveComponent();
    return Component ? Component->GetOwner() : nullptr;
}

FVector FLuaComponentProxy::GetRelativeLocation() const
{
    USceneComponent* Comp = ResolveSceneComponent();
    return Comp ? Comp->GetRelativeLocation() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaComponentProxy::SetRelativeLocation(const FVector& InLocation)
{
    if (USceneComponent* Comp = ResolveSceneComponent())
    {
        Comp->SetRelativeLocation(InLocation);
    }
}

FVector FLuaComponentProxy::GetRelativeScale() const
{
    USceneComponent* Comp = ResolveSceneComponent();
    return Comp ? Comp->GetRelativeScale() : FVector(1.0f, 1.0f, 1.0f);
}

void FLuaComponentProxy::SetRelativeScale(const FVector& InScale)
{
    if (USceneComponent* Comp = ResolveSceneComponent())
    {
        Comp->SetRelativeScale(InScale);
    }
}

FVector FLuaComponentProxy::GetRelativeRotation() const
{
    USceneComponent* Comp = ResolveSceneComponent();
    if (!Comp)
    {
        return FVector(0.0f, 0.0f, 0.0f);
    }

    FRotator Rot = Comp->GetRelativeRotation();
    return FVector(Rot.Pitch, Rot.Yaw, Rot.Roll);
}

void FLuaComponentProxy::SetRelativeRotation(const FVector& InEulerRotation)
{
    if (USceneComponent* Comp = ResolveSceneComponent())
    {
        Comp->SetRelativeRotation(InEulerRotation);
    }
}

bool FLuaComponentProxy::SetVisible(bool bVisible)
{
    UPrimitiveComponent* Comp = ResolvePrimitiveComponent();
    if (!Comp)
    {
        return false;
    }

    Comp->SetVisibility(bVisible);
    return true;
}

bool FLuaComponentProxy::SetVisibleInGame(bool bVisible)
{
    UPrimitiveComponent* Comp = ResolvePrimitiveComponent();
    if (!Comp)
    {
        return false;
    }

    Comp->SetVisibleInGame(bVisible);
    return true;
}

bool FLuaComponentProxy::SetVisibleInEditor(bool bVisible)
{
    UPrimitiveComponent* Comp = ResolvePrimitiveComponent();
    if (!Comp)
    {
        return false;
    }

    Comp->SetVisibleInEditor(bVisible);
    return true;
}

bool FLuaComponentProxy::SetStaticMesh(const FString& MeshPath)
{
    UStaticMeshComponent* MeshComp = ResolveStaticMeshComponent();
    if (!MeshComp)
    {
        return false;
    }

    if (MeshPath.empty() || MeshPath == "None")
    {
        MeshComp->SetStaticMesh(nullptr);
        return true;
    }

    if (!GEngine)
    {
        return false;
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Loaded = FObjManager::LoadObjStaticMesh(MeshPath, Device);
    if (!Loaded)
    {
        return false;
    }

    MeshComp->SetStaticMesh(Loaded);
    return true;
}

FString FLuaComponentProxy::GetStaticMeshPath() const
{
    UStaticMeshComponent* MeshComp = ResolveStaticMeshComponent();
    return MeshComp ? MeshComp->GetStaticMeshPath() : "";
}

int32 FLuaComponentProxy::GetNumMaterials() const
{
    UPrimitiveComponent* Prim = ResolvePrimitiveComponent();
    return Prim ? Prim->GetNumMaterials() : 0;
}

bool FLuaComponentProxy::SetMaterial(int32 ElementIndex, const FString& MaterialPath)
{
    if (USubUVComponent* SubUVComp = ResolveSubUVComponent())
    {
        if (MaterialPath.empty() || MaterialPath == "None")
        {
            SubUVComp->SetSubUVMaterial(nullptr);
            SubUVComp->MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
            return true;
        }

        UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
        if (!Material)
        {
            return false;
        }

        SubUVComp->SetSubUVMaterial(Material);
        SubUVComp->MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
        return true;
    }

    UStaticMeshComponent* MeshComp = ResolveStaticMeshComponent();
    if (!MeshComp)
    {
        return false;
    }

    if (MaterialPath.empty() || MaterialPath == "None")
    {
        MeshComp->SetMaterial(ElementIndex, nullptr);
        return true;
    }

    UMaterial* Material = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(MaterialPath);
    if (!Material)
    {
        return false;
    }

    MeshComp->SetMaterial(ElementIndex, Material);
    return true;
}

bool FLuaComponentProxy::SetSubUVGrid(int32 Rows, int32 Columns)
{
    USubUVComponent* SubUVComp = ResolveSubUVComponent();
    if (!SubUVComp)
    {
        return false;
    }

    SubUVComp->SetCellCount(Columns, Rows);
    return true;
}

bool FLuaComponentProxy::SetCastShadow(bool bCastShadow)
{
    UPrimitiveComponent* Comp = ResolvePrimitiveComponent();
    if (!Comp)
    {
        return false;
    }

    Comp->SetCastShadow(bCastShadow);
    return true;
}

bool FLuaComponentProxy::SetText(const FString& Text)
{
    UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(ResolveComponent());
    if (!TextComp)
    {
        return false;
    }

    TextComp->SetText(Text);
    return true;
}

FString FLuaComponentProxy::GetText() const
{
    UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(ResolveComponent());
    return TextComp ? TextComp->GetText() : "";
}

bool FLuaComponentProxy::SetFontSize(float FontSize)
{
    UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(ResolveComponent());
    if (!TextComp)
    {
        return false;
    }

    TextComp->SetFontSize(FontSize);
    return true;
}

bool FLuaComponentProxy::SetBillboard(bool bBillboard)
{
    UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(ResolveComponent());
    if (!TextComp)
    {
        return false;
    }

    TextComp->SetBillboard(bBillboard);
    return true;
}

bool FLuaComponentProxy::SetIntensity(float Intensity)
{
    ULightComponentBase* Light = Cast<ULightComponentBase>(ResolveComponent());
    if (!Light)
    {
        return false;
    }

    Light->SetIntensity(Intensity);
    return true;
}

bool FLuaComponentProxy::SetLightColor(float R, float G, float B, float A)
{
    ULightComponentBase* Light = Cast<ULightComponentBase>(ResolveComponent());
    if (!Light)
    {
        return false;
    }

    Light->SetLightColor(FVector4(R, G, B, A));
    return true;
}

float FLuaComponentProxy::GetAttenuationRadius() const
{
    UPointLightComponent* Light = Cast<UPointLightComponent>(ResolveComponent());
    return Light ? Light->GetAttenuationRadius() : 0.0f;
}

bool FLuaComponentProxy::SetAttenuationRadius(float Radius)
{
    UPointLightComponent* Light = Cast<UPointLightComponent>(ResolveComponent());
    if (!Light)
    {
        return false;
    }

    Light->SetAttenuationRadius(Radius);
    return true;
}

bool FLuaComponentProxy::SetAffectsWorld(bool bAffectsWorld)
{
    ULightComponentBase* Light = Cast<ULightComponentBase>(ResolveComponent());
    if (!Light)
    {
        return false;
    }

    Light->SetAffectsWorld(bAffectsWorld);
    return true;
}

bool FLuaComponentProxy::SetCastShadows(bool bCastShadows)
{
    ULightComponentBase* Light = Cast<ULightComponentBase>(ResolveComponent());
    if (!Light)
    {
        return false;
    }

    Light->SetCastShadows(bCastShadows);
    return true;
}

bool FLuaComponentProxy::SetSphereRadius(float Radius)
{
    USphereComponent* Sphere = Cast<USphereComponent>(ResolveComponent());
    if (!Sphere)
    {
        return false;
    }

    Sphere->SetRadius(Radius);
    return true;
}

bool FLuaComponentProxy::SetBoxExtent(const FVector& Extent)
{
    UBoxComponent* Box = Cast<UBoxComponent>(ResolveComponent());
    if (!Box)
    {
        return false;
    }

    Box->SetBoxExtent(Extent);
    return true;
}

bool FLuaComponentProxy::SetCapsuleSize(float Radius, float HalfHeight)
{
    UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(ResolveComponent());
    if (!Capsule)
    {
        return false;
    }

    Capsule->SetCapsuleSize(Radius, HalfHeight);
    return true;
}

bool FLuaComponentProxy::SetCapsuleRadius(float Radius)
{
    UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(ResolveComponent());
    if (!Capsule)
    {
        return false;
    }

    Capsule->SetCapsuleRadius(Radius);
    return true;
}

bool FLuaComponentProxy::SetCapsuleHalfHeight(float HalfHeight)
{
    UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(ResolveComponent());
    if (!Capsule)
    {
        return false;
    }

    Capsule->SetCapsuleHalfHeight(HalfHeight);
    return true;
}

FString FLuaComponentProxy::GetPatrolGroup() const
{
    if (UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent()))
    {
        return Agent->GetPatrolGroup();
    }

    if (UPatrolPointComponent* Point = Cast<UPatrolPointComponent>(ResolveComponent()))
    {
        return Point->GetPatrolGroup();
    }

    return "";
}

bool FLuaComponentProxy::SetPatrolGroup(const FString& PatrolGroup)
{
    if (UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent()))
    {
        Agent->SetPatrolGroup(PatrolGroup);
        return true;
    }

    if (UPatrolPointComponent* Point = Cast<UPatrolPointComponent>(ResolveComponent()))
    {
        Point->SetPatrolGroup(PatrolGroup);
        return true;
    }

    return false;
}

int32 FLuaComponentProxy::GetPatrolOrder() const
{
    UPatrolPointComponent* Point = Cast<UPatrolPointComponent>(ResolveComponent());
    return Point ? Point->GetPatrolOrder() : 0;
}

bool FLuaComponentProxy::SetPatrolOrder(int32 PatrolOrder)
{
    UPatrolPointComponent* Point = Cast<UPatrolPointComponent>(ResolveComponent());
    if (!Point)
    {
        return false;
    }

    Point->SetPatrolOrder(PatrolOrder);
    return true;
}

float FLuaComponentProxy::GetPatrolMoveSpeed() const
{
    UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent());
    return Agent ? Agent->GetMoveSpeed() : 0.0f;
}

bool FLuaComponentProxy::SetPatrolMoveSpeed(float MoveSpeed)
{
    UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent());
    if (!Agent)
    {
        return false;
    }

    Agent->SetMoveSpeed(MoveSpeed);
    return true;
}

float FLuaComponentProxy::GetPatrolReachDistance() const
{
    UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent());
    return Agent ? Agent->GetReachDistance() : 0.0f;
}

bool FLuaComponentProxy::SetPatrolReachDistance(float ReachDistance)
{
    UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent());
    if (!Agent)
    {
        return false;
    }

    Agent->SetReachDistance(ReachDistance);
    return true;
}

bool FLuaComponentProxy::IsPatrolLoop() const
{
    UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent());
    return Agent ? Agent->IsLoop() : false;
}

bool FLuaComponentProxy::SetPatrolLoop(bool bLoop)
{
    UPatrolAgentComponent* Agent = Cast<UPatrolAgentComponent>(ResolveComponent());
    if (!Agent)
    {
        return false;
    }

    Agent->SetLoop(bLoop);
    return true;
}

bool FLuaComponentProxy::SetScriptPath(const FString& ScriptPath)
{
    ULuaScriptComponent* LuaScript = Cast<ULuaScriptComponent>(ResolveComponent());
    if (!LuaScript)
    {
        return false;
    }

    LuaScript->SetScriptPath(ScriptPath);

    AActor* OwnerActor = LuaScript->GetOwner();
    if (OwnerActor && OwnerActor->HasActorBegunPlay())
    {
        LuaScript->BeginPlay();
    }

    return true;
}

bool FLuaComponentProxy::CallFunction(const FString& FunctionName, sol::variadic_args Args)
{
    ULuaScriptComponent* LuaScript = Cast<ULuaScriptComponent>(ResolveComponent());
    if (!LuaScript)
    {
        return false;
    }

    return LuaScript->CallFunction(FunctionName, Args);
}
