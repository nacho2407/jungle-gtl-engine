#pragma once

#include "Core/CoreTypes.h"
#include "GameFramework/AActor.h"
#include "LuaScript/LuaIncludes.h"
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

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;
	/*
	 * Transform
	 */

    FVector GetRelativeLocation() const;
    void SetRelativeLocation(const FVector& InLocation);

    FVector GetRelativeScale() const;
    void SetRelativeScale(const FVector& InScale);

    FVector GetRelativeRotation() const;
    void SetRelativeRotation(const FVector& InEulerRotation);

	/*
     * Visibility
     */

    bool SetVisible(bool bVisible);
    bool SetVisibleInGame(bool bVisible);
    bool SetVisibleInEditor(bool bVisible);

	/*
     * Static Mesh
     */

    bool SetStaticMesh(const FString& MeshPath);
    FString GetStaticMeshPath() const;
    int32 GetNumMaterials() const;
    bool SetMaterial(int32 ElementIndex, const FString& MaterialPath);

	/*
     * Text Render
     */

    bool SetText(const FString& Text);
    FString GetText() const;
    bool SetFontSize(float FontSize);

	/**
	 * @brief 빌보드 자체를 설정하는 함수가 아니라 text render component를 빌보드로 설정할지 결정
	 */
    bool SetBillboard(bool bBillboard);

	/*
     * Light
     */

    bool SetIntensity(float Intensity);
    bool SetLightColor(float R, float G, float B, float A = 1.0f);
    bool SetAffectsWorld(bool bAffectsWorld);
    bool SetCastShadows(bool bCastShadows);

	/*
	 * Collision Shape
	 */

    bool SetSphereRadius(float Radius);
	bool SetBoxExtent(const FVector& Extent);
    bool SetCapsuleSize(float Radius, float HalfHeight);
    bool SetCapsuleRadius(float Radius);
    bool SetCapsuleHalfHeight(float HalfHeight);

    bool SetScriptPath(const FString& ScriptPath);
    bool CallFunction(const FString& FunctionName, sol::variadic_args Args);

private:
    USceneComponent* ResolveSceneComponent() const;
    UPrimitiveComponent* ResolvePrimitiveComponent() const;
    UStaticMeshComponent* ResolveStaticMeshComponent() const;
    AActor* ResolveActor() const;

private:
    uint32 ComponentUUID = 0;
};
