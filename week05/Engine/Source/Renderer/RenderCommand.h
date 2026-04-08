#pragma once

#include "CoreMinimal.h"

#include <memory>

struct FRenderMesh;
class FMaterial;

enum class ERenderLayer
{
	Default,
	Overlay,
	Transparent,
};

struct ENGINE_API FRenderCommand
{
	FRenderCommand() = default;
	FRenderCommand(const FRenderCommand& Other);
	FRenderCommand(FRenderCommand&& Other) noexcept;
	FRenderCommand& operator=(const FRenderCommand& Other);
	FRenderCommand& operator=(FRenderCommand&& Other) noexcept;
	~FRenderCommand();

	FRenderMesh* RenderMesh = nullptr;
	// 프레임 중 재생성되는 임시 메시(예: 회전 기즈모)의 수명을 보장한다.
	std::shared_ptr<FRenderMesh> RenderMeshOwner = nullptr;

	FMatrix WorldMatrix;
	FMaterial* Material = nullptr;
	uint64 SortKey = 0;
	uint64 SubmissionOrder = 0;

	uint32 IndexStart = 0;
	uint32 IndexCount = 0;

	// 이 커맨드가 어느 씬 버킷에서 실행될지를 나타낸다.
	ERenderLayer RenderLayer = ERenderLayer::Default;
	bool bDisableDepthTest = false;
	bool bDisableDepthWrite = false;
	bool bDisableCulling = false;

	// 머티리얼과 메시 기준의 기본 정렬 키를 생성한다.
	static uint64 MakeSortKey(const FMaterial* InMaterial, const FRenderMesh* InMeshData);
};

/**
 * 씬 렌더링에 사용할 커맨드를 버킷별로 분리해 보관하는 구조다.
 * Default는 정렬 이득이 큰 일반 메시, Overlay는 기즈모/그리드 같은 덧그리기,
 * Transparent는 이후 별도 정렬 정책이 필요한 투명체용 버킷이다.
 */
struct ENGINE_API FRenderCommandQueue
{
	// 기본 씬 패스에서 정렬 후 실행할 일반 메시 커맨드다.
	TArray<FRenderCommand> DefaultCommands;
	// 일반 씬 뒤에 상대적으로 적은 정렬 비용으로 실행할 오버레이 커맨드다.
	TArray<FRenderCommand> OverlayCommands;
	// 투명체처럼 별도 정렬 정책이 필요한 커맨드 버킷이다.
	TArray<FRenderCommand> TransparentCommands;

	// 이 큐를 만들 때 사용한 카메라 뷰 행렬이다.
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;

	// 전체 예상 개수를 바탕으로 각 버킷 메모리를 미리 확보한다.
	void Reserve(size_t Count)
	{
		DefaultCommands.reserve(Count);
		OverlayCommands.reserve((Count / 4) + 1);
		TransparentCommands.reserve((Count / 4) + 1);
	}

	// 커맨드의 레이어에 맞는 버킷으로 자동 분배한다.
	void AddCommand(const FRenderCommand& Cmd)
	{
		switch (Cmd.RenderLayer)
		{
		case ERenderLayer::Overlay:
			OverlayCommands.push_back(Cmd);
			break;

		case ERenderLayer::Transparent:
			TransparentCommands.push_back(Cmd);
			break;

		case ERenderLayer::Default:
		default:
			DefaultCommands.push_back(Cmd);
			break;
		}
	}

	// 다른 큐의 버킷 내용을 현재 큐 뒤에 붙인다.
	void Append(const FRenderCommandQueue& Other)
	{
		DefaultCommands.insert(DefaultCommands.end(), Other.DefaultCommands.begin(), Other.DefaultCommands.end());
		OverlayCommands.insert(OverlayCommands.end(), Other.OverlayCommands.begin(), Other.OverlayCommands.end());
		TransparentCommands.insert(TransparentCommands.end(), Other.TransparentCommands.begin(), Other.TransparentCommands.end());
	}

	// 큐에 들어 있는 전체 커맨드 수를 반환한다.
	size_t GetTotalCommandCount() const
	{
		return DefaultCommands.size() + OverlayCommands.size() + TransparentCommands.size();
	}

	// 어떤 버킷에도 커맨드가 없으면 true를 반환한다.
	bool IsEmpty() const
	{
		return DefaultCommands.empty() && OverlayCommands.empty() && TransparentCommands.empty();
	}

	// 모든 버킷을 비우고 카메라 행렬은 유지하지 않는다.
	void Clear()
	{
		DefaultCommands.clear();
		OverlayCommands.clear();
		TransparentCommands.clear();
	}
};
