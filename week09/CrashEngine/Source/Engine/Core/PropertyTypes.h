// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include <cstdint>
#include <vector>
#include <string>

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
    Bool,
    ByteBool, // uint8을 bool처럼 사용 (std::vector<bool> 회피용)
    Int,
    Float,
    Vec3,
    Vec4,
    Rotator, // FRotator (Pitch, Yaw, Roll)
    String,
    Name,          // FName — 문자열 풀 기반 이름 (리소스 키 등)
    Color4,        // FVector4 RGBA — ImGui::ColorEdit4 위젯
    StaticMeshRef, // UStaticMesh* 에셋 레퍼런스 (드롭다운 선택)
    MaterialSlot,  // FMaterialSlot — 머티리얼 경로
    ComponentRef,  // 액터 내 SceneComponent 참조 — 계층 경로 문자열 ("Root", "0", "1/0" …)
};

// 머티리얼 슬롯: 경로를 하나의 단위로 관리
struct FMaterialSlot
{
    std::string Path;
};

// 컴포넌트가 노출하는 편집 가능한 프로퍼티 디스크립터
struct FPropertyDescriptor
{
    std::string Name;
    EPropertyType Type;
    void* ValuePtr;

    // float 범위 힌트 (DragFloat 등에서 사용)
    float Min = 0.0f;
    float Max = 0.0f;
    float Speed = 0.1f;
};
