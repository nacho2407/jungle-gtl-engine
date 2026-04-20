#pragma once

#include "Core/CoreTypes.h"

/*
    렌더러가 공통으로 사용하는 상수 버퍼 / 시스템 텍스처 / 샘플러 슬롯 번호 모음입니다.
    HLSL과 C++이 같은 슬롯 번호를 바라보도록 중앙에서 관리합니다.
*/
namespace ECBSlot
{
    // b0 : 프레임 공통 데이터(View / Projection / Camera / Time)
    constexpr uint32 Frame = 0;

    // b1 : 오브젝트 공통 데이터(Model / NormalMatrix / Tint)
    constexpr uint32 PerObject = 1;

    // b2 : 패스/셰이더별 추가 상수 버퍼 0
    constexpr uint32 PerShader0 = 2;

    // b3 : 패스/셰이더별 추가 상수 버퍼 1
    constexpr uint32 PerShader1 = 3;

    // b4 : 전역 라이트 데이터
    constexpr uint32 Light = 4;
}

namespace ESystemTexSlot
{
    // t6 : StructuredBuffer 형태의 로컬 라이트 목록
    constexpr uint32 LocalLights = 6;

    // t10 : 현재 뷰포트 깊이 복사본(SceneDepth / Fog / Decal / Outline 입력)
    constexpr uint32 SceneDepth = 10;

    // t11 : 현재 뷰포트 색상 복사본(Fog / FXAA / Outline 입력)
    constexpr uint32 SceneColor = 11;

    // t13 : 선택 마스크용 스텐실 복사본(Outline 입력)
    constexpr uint32 Stencil = 13;
}

namespace ESamplerSlot
{
    // s0 : Clamp + Linear. 포스트 프로세스, UI, 기본 샘플링용
    constexpr uint32 LinearClamp = 0;

    // s1 : Wrap + Linear. 메시 텍스처와 데칼 샘플링용
    constexpr uint32 LinearWrap = 1;

    // s2 : Clamp + Point. 폰트, 깊이/스텐실 정밀 샘플링용
    constexpr uint32 PointClamp = 2;
}

namespace ECBPoolKey
{
    // 기즈모 전용 상수 버퍼
    constexpr uint32 Gizmo = 0;

    // Height Fog 전용 상수 버퍼
    constexpr uint32 Fog = 2;

    // Outline 후처리 전용 상수 버퍼
    constexpr uint32 Outline = 3;

    // SceneDepth 시각화 전용 상수 버퍼
    constexpr uint32 SceneDepth = 4;

    // FXAA 전용 상수 버퍼
    constexpr uint32 FXAA = 5;

    // 라이팅/라이트 유틸 전용 상수 버퍼
    constexpr uint32 Light = 6;
}
