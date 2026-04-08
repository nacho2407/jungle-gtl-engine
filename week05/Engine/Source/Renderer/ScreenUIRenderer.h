#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/UIDrawList.h"
#include <memory>
#include "Renderer/MeshData.h"
#include "Renderer/Material.h"

class FRenderer;

struct ENGINE_API FUIBatchCommand
{
	// 이 UI 배치 커맨드가 그릴 메시다.
	FDynamicMesh* Mesh = nullptr;
	// 위 메시를 그릴 때 바인딩할 머티리얼이다.
	FMaterial* Material = nullptr;
	// 정렬된 UI 레이어/깊이 키다.
	int32 Layer = 0;
	float Depth = 0.0f;
	int32 DepthSortKey = 0;
};

struct ENGINE_API FUIRenderBatch
{
	// 현재 프레임에 모아둔 UI 메시/머티리얼 쌍이다.
	TArray<FUIBatchCommand> Commands;
	// 화면 공간 UI는 identity view를 사용한다.
	FMatrix ViewMatrix = FMatrix::Identity;
	// 픽셀 좌표를 클립 공간으로 바꾸는 직교 투영 행렬이다.
	FMatrix ProjectionMatrix = FMatrix::Identity;

	// 현재 배치에 쌓인 UI 드로우 커맨드를 모두 비운다.
	void Clear()
	{
		Commands.clear();
		ViewMatrix = FMatrix::Identity;
		ProjectionMatrix = FMatrix::Identity;
	}

	// 예상 UI 커맨드 수만큼 메모리를 미리 확보한다.
	void Reserve(size_t Count)
	{
		Commands.reserve(Count);
	}
};

class ENGINE_API FScreenUIRenderer
{
public:
	FScreenUIRenderer() = default;
	~FScreenUIRenderer();

	// 기록된 드로우 리스트를 GPU 작업으로 바꿔 백버퍼에 그린다.
	bool Render(FRenderer& Renderer, const FUIDrawList& DrawList);
	// 프레임 임시 UI 메시와 배치 상태를 정리한다.
	void ResetFrame();

private:
	// 현재 UI 프레임에서 사용할 임시 메시를 하나 만든다.
	FDynamicMesh* CreateFrameMesh(EMeshTopology Topology);
	// 공용 단색 UI 머티리얼을 반환한다.
	FDynamicMaterial* GetOrCreateColorMaterial(FRenderer& Renderer);
	// 요청한 텍스트 색상에 대응하는 폰트 머티리얼을 반환하거나 생성한다.
	FDynamicMaterial* GetOrCreateFontMaterial(FRenderer& Renderer, uint32 Color);
	// 메시/머티리얼 쌍을 현재 UI 배치에 추가한다.
	void EnqueueMesh(FDynamicMesh* Mesh, FMaterial* Material, int32 Layer, float Depth);
	// 준비된 UI 배치에서 메시/머티리얼 쌍 하나를 그린다.
	bool DrawBatchCommand(FRenderer& Renderer, const FUIBatchCommand& BatchCommand);
	// 채워진 사각형 엘리먼트를 메시 기하로 변환한다.
	void AppendFilledRect(const FUIDrawElement& Element);
	// 사각형 외곽선 엘리먼트를 메시 기하로 변환한다.
	void AppendRectOutline(const FUIDrawElement& Element);
	// 텍스트 엘리먼트를 메시 기하로 변환한다.
	void AppendText(FRenderer& Renderer, const FUIDrawElement& Element);
	// 화면 UI에 사용할 직교 투영 행렬을 만든다.
	void ApplyOrthoProjection(int32 Width, int32 Height);
	// 준비된 UI 배치를 백버퍼에 바로 실행한다.
	bool RenderBatch(FRenderer& Renderer);

private:
	FMatrix OrthoProjection = FMatrix::Identity;
	FUIRenderBatch UIBatch;
	std::unique_ptr<FDynamicMaterial> UIColorMaterial;
	TMap<uint32, std::unique_ptr<FDynamicMaterial>> FontMaterialByColor;
	TArray<std::unique_ptr<FDynamicMesh>> FrameMeshes;
};
