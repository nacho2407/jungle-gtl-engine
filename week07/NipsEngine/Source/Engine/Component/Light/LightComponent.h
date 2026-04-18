#pragma once

#include "LightComponentBase.h"

class ULightComponent : public ULightComponentBase {
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

	ULightComponent() = default;
	~ULightComponent() = default;

	// 렌더 파이프라인과 연결되는 공통 라이트 인터페이스 추가 예정
};