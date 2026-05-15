#pragma once

struct FPoseContext;

// 포즈 연산 정적 라이브러리. UObject 가 아니다.
namespace FAnimationRuntime
{
	// 본별 transform 을 선형 보간.
	//   Location/Scale → Lerp
	//   Rotation       → Slerp (★ 사원수 Lerp 는 잘못된 방향 보간 — 반드시 Slerp)
	//
	// A.Pose.size() == B.Pose.size() == Out.Pose.size() 가정.
	// Alpha 0 → A, 1 → B, [0,1] 외 범위는 호출자 책임.
	void BlendTwoPosesTogether(
		const FPoseContext& A,
		const FPoseContext& B,
		float Alpha,
		FPoseContext& Out);
}
