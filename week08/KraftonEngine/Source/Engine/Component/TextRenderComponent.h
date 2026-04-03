#pragma once

#include "BillboardComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

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

// 텍스트를 월드 공간에 빌보드로 렌더링하는 컴포넌트.
// PrimitiveComponent를 상속받아 RenderCollector에 자동으로 감지됩니다.
// MeshBuffer를 사용하지 않으며, FontBatcher가 드로우콜을 처리합니다.
class UTextRenderComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

	UTextRenderComponent();
	~UTextRenderComponent() override = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// --- Text ---
	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	// Owner의 UUID를 문자열로 반환
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

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	//Collision
	void UpdateWorldAABB() const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	FMatrix CalculateOutlineMatrix() const;
	FMatrix CalculateOutlineMatrix(const FMatrix& BillboardWorldMatrix) const;
	int32 GetUTF8Length(const FString& str) const;

private:
	FString Text;
	FName FontName = FName("Default");
	FFontResource* CachedFont = nullptr;	// ResourceManager 소유, 여기선 참조만

	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float FontSize = 1.0f;
	float Spacing = 0.1f;
	float CharWidth = 0.5f;
	float CharHeight = 0.5f;

	ETextRenderSpace RenderSpace = ETextRenderSpace::World;
	ETextHAlign HAlign = ETextHAlign::Center;
	ETextVAlign VAlign = ETextVAlign::Center;

	// Screen 모드 전용
	float ScreenX = 0.0f;
	float ScreenY = 0.0f;
};
