#pragma once

#include "Math/Matrix.h"
#include "EngineCore/Color.h"
#include "Math/Vector4.h"
#include <d3d11.h>
//#include "tup"
struct FMaterial;
struct FGeometry;
class UPrimitiveComponent;

enum class EEngineShowFlags
{
	SF_Primitives,
	SF_BillboardText,
	SF_EditorPrimitives,
	SF_DebugLines,
	SF_None,
};

struct BatchKey {
	FMaterial* Material;
	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;

	bool operator<(const BatchKey& other) const {
		return std::tie(Material, PrimitiveTopology) <
			std::tie(other.Material, other.PrimitiveTopology);
	}

	// 해시 맵 지원을 위한 동등성 검사
	bool operator==(const BatchKey& other) const
	{
		return Material == other.Material
			&& PrimitiveTopology == other.PrimitiveTopology;
	}
};

/**
 * @brief 해시 지원을 위한 functor
 */
struct FBatchKeyHash
{
	size_t operator()(const BatchKey& Key) const
	{
		size_t Seed = std::hash<FMaterial*>{}(Key.Material);

		const auto HashCombine = [&Seed](size_t Value)
			{
				Seed ^= Value + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
			};

		HashCombine(
			std::hash<int>{}(static_cast<int>(Key.PrimitiveTopology))
		);

		return Seed;
	}
};

struct RenderObject
{
public:
	bool bIsDirty;
	virtual void Render(ID3D11DeviceContext* Context);

public:
	FGeometry* Geometry;//Mesh
	FMaterial* Material;//Material
	FMatrix World = FMatrix::IdentityMatrix();
	FColor Color = FColor(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 UVTransform = FVector4(1.0f, 1.0f, 0.0f, 0.0f);

	BatchKey GetBatchKey() const
	{
		return { Material, PrimitiveTopology };
	}
	//Texture

	bool bIsSelected = false;
	bool bDepthEnabled = true;
	bool bDisableCulling = false;
	bool bBatch = false;
	EEngineShowFlags ShowFlag = EEngineShowFlags::SF_None;
	UPrimitiveComponent* OwnerComponent = nullptr;

	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};
