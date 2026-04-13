#pragma once
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

class FShader;

/*
	GPU Constant Buffer 구조체, 섹션별 드로우 정보 등
	렌더링에 필요한 데이터 타입을 정의합니다.
*/

// HLSL CB 바인딩 슬롯 — b0/b1 고정, b2/b3 셰이더별 여분
namespace ECBSlot
{
	constexpr uint32 Frame = 0; // b0: View/Projection/Wireframe (고정)
	constexpr uint32 PerObject = 1; // b1: Model/Color (고정)
	constexpr uint32 PerShader0 = 2; // b2: 셰이더별 여분 슬롯 #0
	constexpr uint32 PerShader1 = 3; // b3: 셰이더별 여분 슬롯 #1
}

// FConstantBufferPool 조회 키 — 바인딩 슬롯과 독립적인 고유 식별자
// [260413 WJ] : Material 개선 이후 다시 생각해본다. 이Key로 공유 상수버퍼를 얻어오는 형태. (@see FScontantBufferPool::GetBuffer)
namespace ECBPoolKey
{
	constexpr uint32 Gizmo = 0;
	constexpr uint32 Material = 1;
	constexpr uint32 Fog = 2;
	constexpr uint32 Outline = 3;
	constexpr uint32 SceneDepth = 4;
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
	FMatrix InvViewProj;
	float bIsWireframe;
	FVector WireframeColor;
	float Time;
	float _pad[3];
};

struct FMaterialConstants
{
	uint32 bIsUVScroll;
	float _pad[3];
	FVector4 SectionColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

// SubUV UV region — atlas frame offset + size (b2 slot, shared with Gizmo)
struct FSubUVRegionConstants
{
	float U = 0.0f;
	float V = 0.0f;
	float Width = 1.0f;
	float Height = 1.0f;
};

struct FGizmoConstants
{
	FVector4 ColorTint;
	uint32 bIsInnerGizmo;
	uint32 bClicking;
	uint32 SelectedAxis;
	float HoveredAxisOpacity;
	uint32 AxisMask;       // 비트 0=X, 1=Y, 2=Z — 1이면 표시, 0이면 숨김. 0x7=전부 표시
	uint32 _pad[3];
};

// PostProcess Outline CB (b3) — HLSL OutlinePostProcessCB와 1:1 대응
struct FOutlinePostProcessConstants
{
	FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
	float OutlineThickness = 1.0f;
	float Padding[3] = {};
};

struct FSceneDepthPConstants
{
	float Exponent;
	float NearClip;
	float FarClip;
	uint32 Mode;
};


// Height Fog CB (b6) — HLSL FogBuffer와 1:1 대응
struct FFogConstants
{
	FVector4 InscatteringColor;  // 16B
	float Density;               // 4B
	float HeightFalloff;         // 4B
	float FogBaseHeight;         // 4B
	float StartDistance;         // 4B  — 16B boundary
	float CutoffDistance;        // 4B
	float MaxOpacity;            // 4B
	float _pad[2];              // 8B  — 16B boundary
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

	// Buffer/Size/Slot
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
	FVector4 DiffuseColor = { 1.0f, 0.0f, 1.0f, 1.0f };		// 기본 마젠타 색
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	bool bIsUVScroll = false;

	//PerShader
	FConstantBuffer* MaterialCB[2];//	[0]=b2, [1]=b3,
	uint8* CPUData[2];   // CPU 메모리의 실제 값
};

