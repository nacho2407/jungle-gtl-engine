#pragma once

#include "SceneComponent.h"
#include "Object/FName.h"

struct FFontResource;

// 텍스트 렌더링 공간 모드
enum class ETextRenderSpace : int32
{
	World,		// 3D 공간에 빌보드로 렌더링
	Screen		// 2D 스크린 좌표에 고정 렌더링
};

// 텍스트 수평 정렬
enum class ETextHAlign : int32
{
	Left,
	Center,
	Right
};

// 텍스트 수직 정렬
enum class ETextVAlign : int32
{
	Top,
	Center,
	Bottom
};

class UTextRenderComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UTextRenderComponent, USceneComponent)

	UTextRenderComponent() = default;
	~UTextRenderComponent() override = default;

	// --- Text ---
	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	// Owner의 UUID를 문자열로 반환 (Billboard 표시용)
	FString GetOwnerUUIDToString() const;

	// Owner의 FName을 문자열로 반환
	FString GetOwnerNameToString() const;

	// --- Font ---
	// FName 키로 ResourceManager에서 FFontResource*를 찾아 캐싱
	void SetFont(const FName& InFontName);
	const FFontResource* GetFont() const { return CachedFont; }
	const FName& GetFontName() const { return FontName; }

	// --- Appearance ---
	void SetColor(const FVector4& InColor) { Color = InColor; }
	const FVector4& GetColor() const { return Color; }

	void SetFontSize(float InSize) { FontSize = InSize; }
	float GetFontSize() const { return FontSize; }

	// --- Space ---
	void SetRenderSpace(ETextRenderSpace InSpace) { RenderSpace = InSpace; }
	ETextRenderSpace GetRenderSpace() const { return RenderSpace; }

	// Screen 모드 전용: 스크린 좌표 (픽셀)
	void SetScreenPosition(float X, float Y) { ScreenX = X; ScreenY = Y; }
	float GetScreenX() const { return ScreenX; }
	float GetScreenY() const { return ScreenY; }

	// --- Alignment ---
	void SetHorizontalAlignment(ETextHAlign InAlign) { HAlign = InAlign; }
	ETextHAlign GetHorizontalAlignment() const { return HAlign; }

	void SetVerticalAlignment(ETextVAlign InAlign) { VAlign = InAlign; }
	ETextVAlign GetVerticalAlignment() const { return VAlign; }

	// --- Visibility ---
	bool IsVisible() const { return bIsVisible; }
	void SetVisibility(bool bVisible) { bIsVisible = bVisible; }

private:
	FString Text;
	FName FontName = FName("Default");
	FFontResource* CachedFont = nullptr;	// ResourceManager 소유, 여기선 참조만

	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float FontSize = 1.0f;

	ETextRenderSpace RenderSpace = ETextRenderSpace::World;
	ETextHAlign HAlign = ETextHAlign::Center;
	ETextVAlign VAlign = ETextVAlign::Center;

	// Screen 모드 전용
	float ScreenX = 0.0f;
	float ScreenY = 0.0f;

	bool bIsVisible = true;
};
