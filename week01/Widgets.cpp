#include "Widgets.hpp"

void UTextWidget::Render(URenderer& renderer)
{
	/*
		UI
	*/
	DirectX::SimpleMath::Vector2 textSize = renderer.Font->MeasureString(Text.c_str());

	renderer.SpriteBatch->Begin();

	// Screen 좌표 계산
	FVector ScreenCoord = CartesianToScreenCoord(GetLocation(true));

	// DrawString 중앙 정렬: Origin = 텍스트 중앙
	DirectX::XMFLOAT2 origin = { textSize.x / 2.f, textSize.y / 2.f };


	renderer.Font->DrawString(
		renderer.SpriteBatch.get(),
		Text.c_str(),
		DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
		TextColor,
		0.0f,
		origin,                        // 텍스트 중앙 기준
		GetScale() * GetParentScale()
	);

	/*
		renderer.Font->DrawString(
		renderer.SpriteBatch.get(),
		Text.c_str(),
		DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
		DirectX::Colors::White,
		0.0f,
		DirectX::XMFLOAT2(0, 0),                        // 텍스트 중앙 기준
		GetScale() * GetParentScale()
	);
	*/

	renderer.SpriteBatch->End();
}

void UImageWidget::Render(URenderer& renderer)
{
	FVector Location = GetLocation(true);
	renderer.UpdateConstant(Location, 0.f, AxisScale * GetScale() * GetParentScale(), 1.0f, TintColor);
	renderer.UpdateTexture(Texture);
	renderer.RenderRectangle();
}

bool UButtonWidget::IsPressed(HWND hWnd, const UInputManager& InputManager, USoundManager& soundManger)
{
	if (IsOnBound(hWnd) && InputManager.IsMouseReleased())
	{
		soundManger.PlaySFX(TEXT("./Sound/SFX/Click.wav"));
		return true;
	}
	return false;
}

bool UButtonWidget::IsOnBound(HWND hWnd)
{
	POINT lpPoint = {};
	GetCursorPos(&lpPoint);
	ScreenToClient(hWnd, &lpPoint);

	float MouseX = lpPoint.x;
	float MouseY = lpPoint.y;

	FVector CartesianCoord = GetLocation(true);

	FVector ScaledAxisScale = AxisScale * GetScale() * GetParentScale();
	FVector LeftTop = { CartesianCoord.x - ScaledAxisScale.x, CartesianCoord.y + ScaledAxisScale.y, 0.f };
	FVector RightBottom = { CartesianCoord.x + ScaledAxisScale.x, CartesianCoord.y - ScaledAxisScale.y, 0.f };

	LeftTop = { (LeftTop.x + 1.0f) * 0.5f * 1024, (1.f - LeftTop.y) * 0.5f * 1024 , 0 };
	RightBottom = { (RightBottom.x + 1.0f) * 0.5f * 1024,  (1.f - RightBottom.y) * 0.5f * 1024 , 0 };

	return (MouseX > LeftTop.x && MouseX < RightBottom.x && MouseY > LeftTop.y && MouseY < RightBottom.y);
}
