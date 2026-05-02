#include "CollisionManager.h"
#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Component/Shape/ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"
#include "Engine/Render/Scene/Scene.h"
#include "Core/Logging/LogMacros.h"
#include <algorithm> 
#include <cmath>
namespace
{
constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 2.0f * kPi;

void DrawDebugUnityCapsule(FScene* Scene, const FVector& Center, float HalfHeight, float Radius, const FVector& UpVector, const FColor& Color)
{
    if (!Scene)
        return;

    FVector Up = UpVector.Normalized();
    FVector Right(1.0f, 0.0f, 0.0f);
    if (std::abs(Up.Dot(Right)) > 0.98f)
    {
        Right = FVector(0.0f, 1.0f, 0.0f);
    }
    FVector Forward = Up.Cross(Right).Normalized();
    Right = Forward.Cross(Up).Normalized();

    FVector TopCenter = Center + Up * HalfHeight;
    FVector BottomCenter = Center - Up * HalfHeight;

    int32 Segments = 16;
    int32 HalfSegments = Segments / 2;

    // 1. 원통 상/하단 뚜껑 (가로 원 2개)
    for (int32 i = 0; i < Segments; ++i)
    {
        float A1 = kTwoPi * i / Segments;
        float A2 = kTwoPi * (i + 1) / Segments;

        Scene->GetDebugPrimitiveQueue().AddLine(
            TopCenter + Right * (std::cos(A1) * Radius) + Forward * (std::sin(A1) * Radius),
            TopCenter + Right * (std::cos(A2) * Radius) + Forward * (std::sin(A2) * Radius), Color, 0.0f);

        Scene->GetDebugPrimitiveQueue().AddLine(
            BottomCenter + Right * (std::cos(A1) * Radius) + Forward * (std::sin(A1) * Radius),
            BottomCenter + Right * (std::cos(A2) * Radius) + Forward * (std::sin(A2) * Radius), Color, 0.0f);
    }

    // 2. 뚜껑의 반구 돔 (세로 아치 4개)
    for (int32 i = 0; i < HalfSegments; ++i)
    {
        float A1 = kPi * i / HalfSegments;
        float A2 = kPi * (i + 1) / HalfSegments;

        // 상단 아치 2개
        Scene->GetDebugPrimitiveQueue().AddLine(
            TopCenter + Right * (std::cos(A1) * Radius) + Up * (std::sin(A1) * Radius),
            TopCenter + Right * (std::cos(A2) * Radius) + Up * (std::sin(A2) * Radius), Color, 0.0f);
        Scene->GetDebugPrimitiveQueue().AddLine(
            TopCenter + Forward * (std::cos(A1) * Radius) + Up * (std::sin(A1) * Radius),
            TopCenter + Forward * (std::cos(A2) * Radius) + Up * (std::sin(A2) * Radius), Color, 0.0f);

        // 하단 아치 2개 (-Up 사용)
        Scene->GetDebugPrimitiveQueue().AddLine(
            BottomCenter + Right * (std::cos(A1) * Radius) - Up * (std::sin(A1) * Radius),
            BottomCenter + Right * (std::cos(A2) * Radius) - Up * (std::sin(A2) * Radius), Color, 0.0f);
        Scene->GetDebugPrimitiveQueue().AddLine(
            BottomCenter + Forward * (std::cos(A1) * Radius) - Up * (std::sin(A1) * Radius),
            BottomCenter + Forward * (std::cos(A2) * Radius) - Up * (std::sin(A2) * Radius), Color, 0.0f);
    }

    // 3. 기둥 세로 선 4개
    Scene->GetDebugPrimitiveQueue().AddLine(TopCenter + Right * Radius, BottomCenter + Right * Radius, Color, 0.0f);
    Scene->GetDebugPrimitiveQueue().AddLine(TopCenter - Right * Radius, BottomCenter - Right * Radius, Color, 0.0f);
    Scene->GetDebugPrimitiveQueue().AddLine(TopCenter + Forward * Radius, BottomCenter + Forward * Radius, Color, 0.0f);
    Scene->GetDebugPrimitiveQueue().AddLine(TopCenter - Forward * Radius, BottomCenter - Forward * Radius, Color, 0.0f);
}

void DrawDebugOBB(FScene* Scene, const FOBB& OBB, const FColor& Color)
{
    if (!Scene)
        return;

    FVector AxisX = OBB.Rotation.GetForwardVector();
    FVector AxisY = OBB.Rotation.GetRightVector();
    FVector AxisZ = OBB.Rotation.GetUpVector();

    FVector ExtX = AxisX * OBB.Extent.X;
    FVector ExtY = AxisY * OBB.Extent.Y;
    FVector ExtZ = AxisZ * OBB.Extent.Z;

    FVector P0 = OBB.Center + ExtX + ExtY + ExtZ;
    FVector P1 = OBB.Center + ExtX - ExtY + ExtZ;
    FVector P2 = OBB.Center - ExtX - ExtY + ExtZ;
    FVector P3 = OBB.Center - ExtX + ExtY + ExtZ;
    FVector P4 = OBB.Center + ExtX + ExtY - ExtZ;
    FVector P5 = OBB.Center + ExtX - ExtY - ExtZ;
    FVector P6 = OBB.Center - ExtX - ExtY - ExtZ;
    FVector P7 = OBB.Center - ExtX + ExtY - ExtZ;

    // 0.0f = 1프레임 동안만 렌더링 (매 프레임 호출하므로 선이 남지 않음)
    Scene->GetDebugPrimitiveQueue().AddBox(P0, P1, P2, P3, P4, P5, P6, P7, Color, 0.0f);
}
}

void FCollisionManager::RegisterComponent(UPrimitiveComponent* Component)
{
    RegisteredComponents.push_back(Component);
}

void FCollisionManager::UnregisterComponent(UPrimitiveComponent* Component)
{
    auto it = std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Component);

    if (it != RegisteredComponents.end())
    {
        *it = RegisteredComponents.back();
        RegisteredComponents.pop_back();

        // 참고: 만약 순서가 꼭 유지되어야 한다면 아래 코드를 씁니다. (O(N) 비용)
        // RegisteredComponents.erase(it);
    }
}

void FCollisionManager::TickCollision(float DeltaTime, FScene* Scene)
{
    // 1. 디버그 드로우용 색상 배열 (기본값: 모두 초록색)
    std::vector<FColor> DebugColors(RegisteredComponents.size(), FColor(0, 255, 0));

    // 2. 충돌 검사
    for (int i = 0; i < RegisteredComponents.size(); i++)
    {
        UPrimitiveComponent* CompA = RegisteredComponents[i];

        for (int j = i + 1; j < RegisteredComponents.size(); j++)
        {
            UPrimitiveComponent* CompB = RegisteredComponents[j];
			
			if (CompA == CompB)
                continue;

            if (!CompB /* || !CompB->bIsCollisionEnabled */)
                continue;

            // 겹침 판정
            if (CheckOverlap(CompA, CompB))
            {
                // 충돌 이벤트를 날림
                CompA->OnComponentOverlap(CompB);
                CompB->OnComponentOverlap(CompA);

                // ⭐️ 충돌한 녀석들의 색상을 빨간색으로 변경!
                DebugColors[i] = FColor(255, 0, 0);
                DebugColors[j] = FColor(255, 0, 0);

                UE_LOG(Console, Info, "Collision!");
            }
        }
    }

    // 3. 디버그 박스 렌더링 (Scene이 유효할 때만)
    if (Scene)
    {
        for (int i = 0; i < RegisteredComponents.size(); i++)
        {
            UShapeComponent* ShapeComp = static_cast<UShapeComponent*>(RegisteredComponents[i]);
            FCollision* Col = ShapeComp->GetCollision();

            if (!Col)
                continue;

            // 타입에 맞춰서 디버그 도형을 그립니다.
            if (Col->GetType() == ECollisionType::Box)
            {
                FBoxCollision* BoxCol = static_cast<FBoxCollision*>(Col);
                DrawDebugOBB(Scene, BoxCol->Bounds, DebugColors[i]);
            }
            else if (Col->GetType() == ECollisionType::Sphere)
            {
                FSphereCollision* SphereCol = static_cast<FSphereCollision*>(Col);
                Scene->GetDebugPrimitiveQueue().AddSphere(SphereCol->Sphere.Center, SphereCol->Sphere.Radius, 16, DebugColors[i], 0.0f);
            }
            else if (Col->GetType() == ECollisionType::Capsule)
            {
                FCapsuleCollision* CapsuleCol = static_cast<FCapsuleCollision*>(Col);
                DrawDebugUnityCapsule(Scene, CapsuleCol->Center, CapsuleCol->HalfHeight, CapsuleCol->Radius, CapsuleCol->UpVector, DebugColors[i]);
            }
        }
    }

}

bool FCollisionManager::CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B)
{
    if (!A->IsA<UShapeComponent>() || !B->IsA<UShapeComponent>())
        return false;

	FCollision* CollisionA = static_cast<UShapeComponent*>(A)->GetCollision();
    FCollision* CollisionB = static_cast<UShapeComponent*>(B)->GetCollision();

    return CollisionA->IsOverlapping(CollisionB); // Intersect 함수는 수학 라이브러리에 구현

}


