#pragma once

/*
	Constants Buffer에 사용될 구조체와
	에 담길 RenderCommand 구조체를 정의하고 있습니다.
	RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Types/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

class FShader;

// HLSL Common.hlsl과 1:1 대응하는 CB 슬롯 정의
namespace ECBSlot
{
	constexpr uint32 Frame     = 0;  // b0: View/Projection/Wireframe
	constexpr uint32 PerObject = 1;  // b1: Model/Color
	constexpr uint32 Gizmo     = 2;  // b2: Gizmo state
	constexpr uint32 Editor    = 4;  // b4: Camera position
	constexpr uint32 Outline   = 5;  // b5: Outline params
}

//PerObject
struct FPerObjectConstants
{
	FMatrix Model;
	FVector4 Color;

	// 기본 PerObject: WorldMatrix + White
	static FPerObjectConstants FromWorldMatrix(const FMatrix& WorldMatrix)
	{
		return { WorldMatrix, FVector4(1.0f, 1.0f, 1.0f, 1.0f) };
	}
};

struct FFrameConstants
{
	FMatrix View;
	FMatrix Projection;
	float bIsWireframe;
	FVector WireframeColor;
};

struct FGizmoConstants
{
	FVector4 ColorTint;
	uint32 bIsInnerGizmo;
	uint32 bClicking;
	uint32 SelectedAxis;
	float HoveredAxisOpacity;
};

struct FEditorConstants
{
	FVector CameraPosition; // xyz 사용, w padding
	uint32 Flag;
};

struct FOutlineConstants
{
	FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
	FVector OutlineInvScale;
	float OutlineOffset = 0.05f;
	uint32 bIs3D; // 0: 2D (Plane 등), 1: 3D (Cube 등)
	float Padding0[3];
};

struct FAABBConstants
{
	FVector Min;
	float Padding0;

	FVector Max;
	float Padding1;

	FColor Color;
};

struct FGridConstants
{
	float GridSpacing;
	int32 GridHalfLineCount;
	float Padding0[2];
};

struct FFontConstants
{
	const FString* Text = nullptr;			// 컴포넌트 소유 문자열 참조 (프레임 내 유효)
	const FFontResource* Font = nullptr;
	float Scale = 1.0f;

	uint32 bScreenSpace = 0;	// true면 스크린 공간에서 렌더링, false면 월드 공간
	FVector2 ScreenPosition = FVector2(0.0f, 0.0f);		// 스크린 공간에서의 위치 (bScreenSpace가 true일 때 사용)
};

struct FSubUVConstants
{
	const FParticleResource* Particle = nullptr;
	uint32 FrameIndex = 0;
	float Width  = 1.0f;
	float Height = 1.0f;
};

// ============================================================
// Batcher Entry — 각 Batcher가 필요한 데이터만 담는 경량 구조체
// ============================================================

struct FFontEntry
{
	FPerObjectConstants PerObject;
	FFontConstants Font;
};

struct FSubUVEntry
{
	FPerObjectConstants PerObject;
	FSubUVConstants SubUV;
};

struct FAABBEntry
{
	FAABBConstants AABB;
};

struct FGridEntry
{
	FGridConstants Grid;
};

// 스크린 공간 텍스트 입력 — Overlay Stats 등에서 사용
struct FScreenTextItem
{
	const FString* Text = nullptr;
	FVector2 ScreenPosition = FVector2(0.0f, 0.0f);
};

// ============================================================
// 타입별 CB 바인딩 디스크립터 — GPU CB에 업로드할 데이터를 인라인 보관
// ============================================================
struct FConstantBufferBinding
{
	FConstantBuffer* Buffer = nullptr;	// 업데이트할 CB (nullptr이면 미사용)
	uint32 Size = 0;					// 업로드할 바이트 수
	uint32 Slot = 0;					// VS/PS CB 슬롯

	static constexpr size_t kMaxDataSize = 64;
	alignas(16) uint8 Data[kMaxDataSize] = {};

	// Buffer/Size/Slot をセットし、Data を T& で返す — Size 不一致を防止
	template<typename T>
	T& Bind(FConstantBuffer* InBuffer, uint32 InSlot)
	{
		static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
		Buffer = InBuffer;
		Size = sizeof(T);
		Slot = InSlot;
		return *reinterpret_cast<T*>(Data);
	}

	template<typename T>
	T& As()
	{
		static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
		return *reinterpret_cast<T*>(Data);
	}

	template<typename T>
	const T& As() const
	{
		static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
		return *reinterpret_cast<const T*>(Data);
	}
};

// 섹션별 드로우 정보 — 머티리얼(텍스처)이 다른 구간을 분리 드로우
struct FMeshSectionDraw
{
	ID3D11ShaderResourceView* DiffuseSRV = nullptr;
	FVector4 DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
};

struct FRenderCommand
{
	FMeshBuffer* MeshBuffer = nullptr;
	FShader* Shader = nullptr;
	FPerObjectConstants PerObjectConstants = {};	// b1 (공통)

	// StaticMesh 섹션별 드로우 정보
	TArray<FMeshSectionDraw> SectionDraws;

	// GPU CB 바인딩 — ExtraCB.Data를 지정 슬롯의 CB에 업로드 (Gizmo, Outline 등)
	FConstantBufferBinding ExtraCB;
};
