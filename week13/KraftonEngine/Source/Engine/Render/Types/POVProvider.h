#pragma once

struct FMinimalViewInfo;

// ============================================================
// IPOVProvider — World 가 Editor viewport 의 POV 를 pull 모델로 받기 위한 인터페이스.
//
// Editor mode 에서는 PC 가 없어 World 가 Active POV 를 직접 알 수 없다. EditorViewportClient
// 가 이 인터페이스를 implement 하고 World->SetEditorPOVProvider(this) 로 등록하면, World 의
// GetActivePOV 가 매 호출마다 active viewport 의 POV 를 pull 한다 (push 캐시 X).
//
// 이로써 World 가 Editor 전용 데이터 (POV cache) 를 보유하지 않게 된다 — 책임 분리.
// ============================================================
class IPOVProvider
{
public:
	virtual ~IPOVProvider() = default;

	// 현재 POV 를 OutPOV 에 채운다. 유효하면 true.
	virtual bool GetCameraView(FMinimalViewInfo& OutPOV) const = 0;
};
