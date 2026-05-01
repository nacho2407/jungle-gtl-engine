#pragma once
#include "Math/Vector.h"
#include "Core/CoreTypes.h"
#include "Core/PropertyTypes.h"

class AActor;
class UPrimitiveComponent;

// ============================================================
// ECollisionChannel — 충돌 채널 (오브젝트 분류용)
// ============================================================
enum class ECollisionChannel : uint8
{
	WorldStatic = 0,
	WorldDynamic = 1,
	Pawn = 2,
	Projectile = 3,
	Trigger = 4,
	// 필요 시 확장 (ActiveCount, MAX 갱신)

	ActiveCount = 5, // 에디터/드롭다운에 노출되는 실질 채널 수
	MAX = 16         // 응답 테이블 최대 슬롯 수
};

// 채널 이름 문자열 (에디터 Enum 드롭다운용)
inline const char* GCollisionChannelNames[] =
{
	"WorldStatic",
	"WorldDynamic",
	"Pawn",
	"Projectile",
	"Trigger",
	"Channel5", "Channel6", "Channel7",
	"Channel8", "Channel9", "Channel10", "Channel11",
	"Channel12", "Channel13", "Channel14", "Channel15"
};

// ============================================================
// ECollisionResponse — 채널 간 응답 방식
// ============================================================
enum class ECollisionResponse : uint8
{
	Ignore = 0,
	Overlap = 1,
	Block = 2,

	COUNT
};

inline const char* GCollisionResponseNames[] =
{
	"Ignore",
	"Overlap",
	"Block"
};

// ============================================================
// ECollisionEnabled — 충돌 활성화 모드
// ============================================================
enum class ECollisionEnabled : uint8
{
	NoCollision = 0,
	QueryOnly = 1,		// Overlap/Hit 이벤트만
	PhysicsOnly = 2,	// 향후 물리 엔진용
	QueryAndPhysics = 3,

	COUNT
};

inline const char* GCollisionEnabledNames[] =
{
	"NoCollision",
	"QueryOnly",
	"PhysicsOnly",
	"QueryAndPhysics"
};

// ============================================================
// FCollisionResponseContainer — 채널별 응답 테이블
// ============================================================
struct FCollisionResponseContainer
{
	ECollisionResponse Responses[static_cast<int32>(ECollisionChannel::MAX)];

	FCollisionResponseContainer()
	{
		SetAllChannels(ECollisionResponse::Block);
	}

	explicit FCollisionResponseContainer(ECollisionResponse DefaultResponse)
	{
		SetAllChannels(DefaultResponse);
	}

	void SetAllChannels(ECollisionResponse InResponse)
	{
		for (int32 i = 0; i < static_cast<int32>(ECollisionChannel::MAX); ++i)
		{
			Responses[i] = InResponse;
		}
	}

	void SetResponse(ECollisionChannel Channel, ECollisionResponse InResponse)
	{
		Responses[static_cast<int32>(Channel)] = InResponse;
	}

	ECollisionResponse GetResponse(ECollisionChannel Channel) const
	{
		return Responses[static_cast<int32>(Channel)];
	}

	// 에디터용 자기기술 함수 — Struct 프로퍼티 시스템에서 사용
	static void DescribeProperties(void* Ptr, std::vector<FPropertyDescriptor>& OutProps)
	{
		auto* Container = static_cast<FCollisionResponseContainer*>(Ptr);
		for (int32 i = 0; i < static_cast<int32>(ECollisionChannel::ActiveCount); ++i)
		{
			FPropertyDescriptor Desc;
			Desc.Name = GCollisionChannelNames[i];
			Desc.Type = EPropertyType::Enum;
			Desc.ValuePtr = &Container->Responses[i];
			Desc.EnumNames = GCollisionResponseNames;
			Desc.EnumCount = static_cast<uint32>(ECollisionResponse::COUNT);
			Desc.EnumSize = sizeof(ECollisionResponse);
			OutProps.push_back(Desc);
		}
	}
};

// ============================================================
// FHitResult — 충돌/레이캐스트 결과
// ============================================================
struct FHitResult
{
	UPrimitiveComponent* HitComponent = nullptr;
	AActor* HitActor = nullptr;

	float Distance = 3.402823466e+38F; // FLT_MAX
	float PenetrationDepth = 0.0f;
	FVector WorldHitLocation = { 0, 0, 0 };
	FVector WorldNormal = { 0, 0, 0 };
	FVector ImpactNormal = { 0, 0, 0 };
	int FaceIndex = -1;

	bool bHit = false;
};

// ============================================================
// FOverlapResult — 오버랩 결과
// ============================================================
struct FOverlapResult
{
	AActor* OverlapActor = nullptr;
	UPrimitiveComponent* OverlapComponent = nullptr;
};

// ============================================================
// FOverlapPair — 프레임 간 오버랩 쌍 추적용
// ============================================================
struct FOverlapPair
{
	UPrimitiveComponent* A = nullptr;
	UPrimitiveComponent* B = nullptr;

	bool operator==(const FOverlapPair& Other) const
	{
		return (A == Other.A && B == Other.B)
			|| (A == Other.B && B == Other.A);
	}
};

// std::unordered_set 호환 해시
namespace std
{
	template<>
	struct hash<FOverlapPair>
	{
		size_t operator()(const FOverlapPair& Pair) const
		{
			// 순서 무관 해시: A와 B를 정렬 후 조합
			auto PtrA = reinterpret_cast<uintptr_t>(Pair.A);
			auto PtrB = reinterpret_cast<uintptr_t>(Pair.B);
			if (PtrA > PtrB) std::swap(PtrA, PtrB);
			size_t H = hash<uintptr_t>()(PtrA);
			H ^= hash<uintptr_t>()(PtrB) + 0x9e3779b9 + (H << 6) + (H >> 2);
			return H;
		}
	};
}
