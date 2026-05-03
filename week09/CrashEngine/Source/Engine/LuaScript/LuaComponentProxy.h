#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class UActorComponent;
class USceneComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

class FLuaComponentProxy
{
public:
    FLuaComponentProxy() = default;
    explicit FLuaComponentProxy(UActorComponent* InComponent);

    void SetComponent(UActorComponent* InComponent);
    UActorComponent* ResolveComponent() const;

    bool IsValid() const;
    FString GetComponentClassName() const;
    uint32 GetUUID() const { return ComponentUUID; }

    FVector GetRelativeLocation() const;
    void SetRelativeLocation(const FVector& InLocation);

    FVector GetRelativeScale() const;
    void SetRelativeScale(const FVector& InScale);

    FVector GetRelativeRotation() const;
    void SetRelativeRotation(const FVector& InEulerRotation);

    bool SetVisible(bool bVisible);
    bool SetVisibleInGame(bool bVisible);
    bool SetVisibleInEditor(bool bVisible);

    bool SetStaticMesh(const FString& MeshPath);
    FString GetStaticMeshPath() const;
    int32 GetNumMaterials() const;
    bool SetMaterial(int32 ElementIndex, const FString& MaterialPath);

    bool SetText(const FString& Text);
    FString GetText() const;
    bool SetFontSize(float FontSize);
    bool SetBillboard(bool bBillboard);

    bool SetIntensity(float Intensity);
    bool SetLightColor(float R, float G, float B, float A = 1.0f);
    bool SetAffectsWorld(bool bAffectsWorld);
    bool SetCastShadows(bool bCastShadows);

    bool SetSphereRadius(float Radius);

private:
    USceneComponent* ResolveSceneComponent() const;
    UPrimitiveComponent* ResolvePrimitiveComponent() const;
    UStaticMeshComponent* ResolveStaticMeshComponent() const;

private:
    uint32 ComponentUUID = 0;
};