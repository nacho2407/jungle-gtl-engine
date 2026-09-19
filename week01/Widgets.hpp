#pragma once

#include <string>

// From DirectXTK
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "SimpleMath.h"

#include "Core.h"

#include "UInputManager.hpp"
#include "URenderer.hpp"
#include "USoundManager.hpp"

class UWidget
{
public:
	UWidget() :
		Parent(nullptr),
		Scale(1)
	{

	}

	void SetParent(UWidget* InWidget)
	{
		// Offset 은 Scale = 1 기준이므로 Scale 을 배제한 GetLocation 사용
		if (InWidget)
		{
			FVector PrevLocation = GetLocation(false);
			Parent = InWidget;
			Offset = PrevLocation - Parent->GetLocation(false);
		}
		else
		{
			if (Parent)
			{
				// parent 초기화하기 전에 현재 위치를 Offset 으로 지정 (parent == nullptr 일 때 Offset 은 location 임을 기억)
				Offset = GetLocation(false);
				Parent = nullptr;
			}
			else
			{
				// do nothing
			}
		}
	}

	void SetScale(float InScale) { Scale = InScale; }

	// Scale = 1 기준, Parent 와의 Offset 설정
	// Parent == nullptr 인 경우 Location 설정과 같음
	// Offset = Self.Loc - Parent.Loc
	// Self.Loc = Parent.Loc + Offset
	void SetOffset(FVector InOffset) { Offset = InOffset; }

	FVector GetLocation(bool bApplyScale) const
	{
		if (Parent)
		{
			FVector Location = Parent->GetLocation(bApplyScale);
			if (bApplyScale) Location = Location + Offset * GetParentScale();
			return Location;
		}
		else
		{
			return Offset;
		}
	}

	float GetParentScale() const
	{
		if (Parent)
		{
			return Parent->GetScale() * Parent->GetParentScale();
		}
		else
		{
			return 1;
		}
	}

	float GetScale() const
	{
		return Scale;
	}

	virtual void Render(URenderer& renderer)
	{

	}

protected:
	// Cartesian <-> Screen 변환 과정을 어느 객체에서 또는 함수에서 할 것인가는 논의 필요
	FVector CartesianToScreenCoord(FVector CartesianCoord)
	{
		// Screen Size 가 (1024, ) 고정 상황 가정
		return FVector((CartesianCoord.x + 1.0f) * 0.5f * 1024, (1 - CartesianCoord.y) * 0.5f * 1024);
	}

private:
	UWidget* Parent;
	float Scale;  // 실제 Offset 에 영향을 줌

	// Parent == nullptr 인 경우 Offset 이 곧 Location 이 됨
	// Parent != nullptr 인 경우 Parent 로부터의 상대적 거리를 나타내는 Offset (Scale = 1 기준)
	FVector Offset;
};

/*
 * UTextWidget Render 후 UImageWidget Render 가 제대로 작동하지 않거나
 * Parent-Child 위치 관계가 보존이 안되는 등의 문제가 있어 현재로썬 단일 객체 + Rendering 순서
 */
class UTextWidget : public UWidget
{
public:
	UTextWidget() :
		TextColor(DirectX::Colors::White)
	{
	}

	void Render(URenderer& renderer) override;

	void SetNumber(int Num)
	{
		Text = std::to_wstring(Num);
	}

	void SetText(const wchar_t* InText)
	{
		Text = InText;
	}

	void SetTextColor(DirectX::XMVECTORF32 InTextColor)
	{
		TextColor = InTextColor;
	}

private:
	std::wstring Text;

	DirectX::XMVECTORF32 TextColor;
};

class UImageWidget : public UWidget
{
public:
	UImageWidget() :
		Texture(nullptr),
		AxisScale(FVector::Splat(1)),
		TintColor(FVector(1, 1, 1))
	{
	}

	void Render(URenderer& renderer) override;

	void SetTexture(UTexture* InTexture) { Texture = InTexture; }
	void SetAxisScale(FVector InAxisScale) { AxisScale = InAxisScale; }
	void SetTintColor(FVector InTintColor) { TintColor = InTintColor; }
	FVector GetTintColor() { return TintColor; }

protected:
	UTexture* Texture;
	FVector AxisScale;
	FVector TintColor;
};

/*
 * 이미지가 없는 버튼도 있겠지만 이 게임 프로토타입에선 배제하여 이미지를 상속받음
 * 버튼의 가운데에 해당 텍스트를 넣는다면 렌더 순서가 꼬여 소유하지 않고 일단 외부 텍스트를 받도록 함
 */
class UButtonWidget : public UImageWidget
{
public:
	UButtonWidget() = default;

	bool IsPressed(HWND hWnd, const UInputManager& InputManager, USoundManager& soundManger);

	/*
	* 스크린 상에서 버튼 클릭 체크
	*/
	bool IsOnBound(HWND hWnd);
};
