#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

class UEditorEngine;
struct FRect;

// 스크린 공간 텍스트 — Overlay Stats 등에서 사용
struct FOverlayStatLine
{
	FString Text;
	FVector2 ScreenPosition = FVector2(0.0f, 0.0f);
};

struct FOverlayStatLayout
{
	float StartX = 16.0f;
	float StartY = 25.0f;
	float LineHeight = 20.0f;
	float GroupSpacing = 12.0f;
	float TextScale = 1.0f;
};

class FOverlayStatSystem
{
public:
	FOverlayStatSystem() = default;

	void ShowFPS(bool bEnable = true) { bShowFPS = bEnable; }
	void ShowPickingTime(bool bEnable = true) { bShowPickingTime = bEnable; }
	void ShowMemory(bool bEnable = true) { bShowMemory = bEnable; }
	void ShowShadow(bool bEnable = true) { bShowShadow = bEnable; }
	void ShowSkinning(bool bEnable = true) { bShowSkinning = bEnable; }
	void ShowParticles(bool bEnable = true) { bShowParticles = bEnable; }
	void ShowPhysX(bool bEnable = true) { bShowPhysX = bEnable; }
	void ShowRagdoll(bool bEnable = true) { bShowRagdoll = bEnable; }
	void ShowCloth(bool bEnable = true) { bShowCloth = bEnable; }
	bool ToggleFPS() { bShowFPS = !bShowFPS; return bShowFPS; }
	bool ToggleMemory() { bShowMemory = !bShowMemory; return bShowMemory; }
	bool ToggleShadow() { bShowShadow = !bShowShadow; return bShowShadow; }
	bool ToggleSkinning() { bShowSkinning = !bShowSkinning; return bShowSkinning; }
	bool ToggleParticles() { bShowParticles = !bShowParticles; return bShowParticles; }
	bool TogglePhysX() { bShowPhysX = !bShowPhysX; return bShowPhysX; }
	bool ToggleRagdoll() { bShowRagdoll = !bShowRagdoll; return bShowRagdoll; }
	bool ToggleCloth() { bShowCloth = !bShowCloth; return bShowCloth; }
	void RecordPickingAttempt(double ElapsedMs);
	void HideAll()
	{
		bShowFPS = false;
		bShowPickingTime = false;
		bShowMemory = false;
		bShowShadow = false;
		bShowSkinning = false;
		bShowParticles = false;
		bShowPhysX = false;
		bShowRagdoll = false;
		bShowCloth = false;
	}

	const FOverlayStatLayout& GetLayout() const { return Layout; }
	FOverlayStatLayout& GetLayout() { return Layout; }

	void BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const;
	TArray<FOverlayStatLine> BuildLines(const UEditorEngine& Editor) const;
	void RenderImGui(const UEditorEngine& Editor, const FRect& ViewportRect) const;

private:
	void AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Text) const;
	void BuildFPSLines(const UEditorEngine& Editor, TArray<FString>& OutLines) const;
	void BuildMemoryLines(TArray<FString>& OutLines) const;
	void BuildShadowLines(TArray<FString>& OutLines) const;
	void BuildSkinningLines(TArray<FString>& OutLines) const;
	void BuildParticleLines(const UEditorEngine& Editor, TArray<FString>& OutLines) const;
	void BuildPhysXLines(TArray<FString>& OutLines) const;
	void BuildRagdollLines(TArray<FString>& OutLines) const;
	void BuildClothLines(TArray<FString>& OutLines) const;

	bool bShowFPS = false;
	bool bShowPickingTime = false; // WM_LBUTTONDOWN , VK_LBUTTON 입력 시점이 아닌 오브젝트 충돌 판정에 걸린 시간을 측정합니다.
	bool bShowMemory = false;
	bool bShowShadow = false;
	bool bShowSkinning = false;
	bool bShowParticles = false;
	bool bShowPhysX = false;
	bool bShowRagdoll = false;
	bool bShowCloth = false;
	double LastPickingTimeMs = 0.0;
	double AccumulatedPickingTimeMs = 0.0;
	uint32 PickingAttemptCount = 0;
	mutable FString CachedFPSLine;
	mutable FString CachedPickingLine;
	mutable double FPSAverageWindowStartTime = 0.0;
	mutable double FPSAccumulatedFrameTimeMs = 0.0;
	mutable uint32 FPSAccumulatedFrameCount = 0;
	mutable bool bFPSAverageInitialized = false;

	struct FSkinningOverlaySample
	{
		bool bValid = false;
		bool bLive = false;
		double LastMs = 0.0;
		double AvgMs = 0.0;
		uint32 CallCount = 0;
	};

	mutable FSkinningOverlaySample CPUVertexSkinSample;
	mutable FSkinningOverlaySample GPUMatrixUploadSample;
	mutable FSkinningOverlaySample SkeletalPreDepthCPUPathSample;
	mutable FSkinningOverlaySample SkeletalPreDepthGPUPathSample;

	FOverlayStatLayout Layout;
};
