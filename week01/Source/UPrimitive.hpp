#pragma once

#include "URenderer.hpp"
#include "UTexture.hpp"

class UPrimitive
{
public:
	UPrimitive(UTexture* InTexture = nullptr) : Texture(InTexture)
	{
	}
	virtual ~UPrimitive() = default; // 생성과 해제를 UPrimitive*로 할 예정이므로

	virtual void UpdatePhysics(float dt) = 0;
	virtual void Render(URenderer& renderer) = 0;

	void SetTexture(UTexture* InTexture) { Texture = InTexture; }

	UTexture* GetTexture() const { return Texture; }

	// 삭제 예정 플래그 조작
	void MarkPendingDestroy() { bPendingDestroy = true; }

	bool IsPendingDestroy() const { return bPendingDestroy; }

protected:
	// Border 는 map 데이터이므로 따로 빼서 전역 상수 등으로 관리하는 것이 좋을 듯함
	// 이외 데이터도 관련해서 수정해야할수도
	float LeftBorder = -0.76f;
	float RightBorder = 0.76f;
	float TopBorder = 1.0f;
	float BottomBorder = -0.9f;

	bool bPendingDestroy = false; // 충돌 검사 중 삭제될 예정인 Primitive들을 표시하기 위한 플래그

private:
	UTexture* Texture;
};
