#pragma once

/*

	는 Renderer에게 Draw Call 요청을 vector의 형태로 전달하는 역할을 합니다.
	Renderer가 RenderBus에 담긴 Draw Call 요청들을 처리할 수 있게 합니다.
*/

//	TODO : CoreType.h 경로 변경 요구
#include "Core/CoreMinimal.h"
#include "Render/Scene/RenderCommand.h"

#include "Render/Common/ViewTypes.h"
#include "Render/Resource/ShaderHelper.h"

class UDirectionalLightComponent;

class FRenderBus
{
public:
	void Clear();
	void AddCommand(ERenderPass Pass, const FRenderCommand& InCommand);
	void AddCommand(ERenderPass Pass, FRenderCommand&& InCommand);
	const TArray<FRenderCommand>& GetCommands(ERenderPass Pass) const;

	// Getter,Setter
	void SetViewProjection(const FMatrix& InView, const FMatrix& InProj, float InNearPlane, float InFarPlane);
	void SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags);

	const FMatrix& GetView() const { return View; }
	const FMatrix& GetProj() const { return Proj; }
	const FVector& GetCameraPosition() const { return CameraPosition;  }
	const FVector& GetCameraForward() const { return CameraForward; }
	const FVector& GetCameraUp() const { return CameraUp; }
	const FVector& GetCameraRight() const { return CameraRight; }
	EViewMode GetViewMode() const { return ViewMode; }
	FShowFlags GetShowFlags() const { return ShowFlags; }
	ELightCullMode GetLightCullMode() const { return LightCullMode; }
	void SetLightCullMode(ELightCullMode InMode) { LightCullMode = InMode; }
	EShadowFilter GetShadowFilterMode() const { return ShadowFilterMode; }
	void SetShadowFilterMode(EShadowFilter InMode) { ShadowFilterMode = InMode; }
	const FVector& GetWireframeColor() const { return WireframeColor; }
	void SetWireframeColor(const FVector& InColor) { WireframeColor = InColor; }
	bool GetFXAAEnabled() const { return bFXAAEnabled; }
	void SetFXAAEnabled(bool bInEnabled) { bFXAAEnabled = bInEnabled; }
	bool GetCascadeVis() const { return bCascadeVis; }
	void SetCascadeVis(bool bInEnabled) { bCascadeVis = bInEnabled; }
	bool IsOrthographic() const { return Proj.M[3][3] == 1.0f; }
	void SetViewportSize(const FVector2& InViewportSize) { ViewportSize = InViewportSize; }
	const FVector2& GetViewportSize() const { return ViewportSize; }
	void SetViewportOrigin(const FVector2& InViewportOrigin) { ViewportOrigin = InViewportOrigin; }
	const FVector2& GetViewportOrigin() const { return ViewportOrigin; }
    float GetNearPlane() const { return NearPlane; }
    float GetFarPlane() const { return FarPlane; }
    void SetVignette(float Intensity, float Radius, float Smoothness)
    {
        VignetteIntensity = Intensity;
        VignetteRadius = Radius;
        VignetteSmoothness = Smoothness;
    }
    float GetVignetteIntensity() const { return VignetteIntensity; }
    float GetVignetteRadius() const { return VignetteRadius; }
    float GetVignetteSmoothness() const { return VignetteSmoothness; }
    void SetCameraFade(const FVector4& InColor, float InAlpha)
    {
        CameraFadeColor = InColor;
        CameraFadeAlpha = InAlpha;
    }
    const FVector4& GetCameraFadeColor() const { return CameraFadeColor; }
    float GetCameraFadeAlpha() const { return CameraFadeAlpha; }
    void SetLetterbox(float InTargetAspect, float InAmount)
    {
        LetterboxTargetAspect = InTargetAspect;
        LetterboxAmount = InAmount;
    }
    float GetLetterboxTargetAspect() const { return LetterboxTargetAspect; }
    float GetLetterboxAmount() const { return LetterboxAmount; }

    bool bSandevistanEnabled = false;
    float SandevistanIntensity = 0.0f;

    bool IsSandevistanEnabled() const { return bSandevistanEnabled; }
    float GetSandevistanIntensity() const { return SandevistanIntensity; }

public:
	// Light
	FAmbientLightInfo		AmbientLightInfo;
	FDirectionalLightInfo	DirectionalLightInfo;
	TArray<FLightInfo>		LightInfos;

	TArray<FShadowLightRequest> ShadowLightRequests;

private:
	TArray<FRenderCommand> PassQueues[(uint32)ERenderPass::MAX];

	FMatrix  View;
	FMatrix  Proj;
	FVector  CameraPosition;
	FVector  CameraForward;
	FVector  CameraRight;
	FVector  CameraUp;
	float	 NearPlane = 0.1f;
	float	 FarPlane = 1000.0f;
	FVector2 ViewportSize;
	FVector2 ViewportOrigin = FVector2(0.0f, 0.0f);
    float VignetteIntensity = 0.0f;
    float VignetteRadius = 0.75f;
    float VignetteSmoothness = 0.35f;
    FVector4 CameraFadeColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    float CameraFadeAlpha = 0.0f;
    float LetterboxTargetAspect = 0.0f;
    float LetterboxAmount = 0.0f;

	//Editor Settings
	EViewMode		ViewMode		= EViewMode::Lit_BlinnPhong;
	ELightCullMode	LightCullMode	= ELightCullMode::Clustered;
	EShadowFilter	ShadowFilterMode = EShadowFilter::PCF;
	FShowFlags		ShowFlags;
	FVector			WireframeColor	= FVector(1.0f, 1.0f, 1.0f);
	bool			bFXAAEnabled	= true;
	bool			bCascadeVis		= false;
};
