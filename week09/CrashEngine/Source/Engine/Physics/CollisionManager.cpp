#include "CollisionManager.h"
#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Component/Shape/ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"
#include "Engine/Render/Scene/Scene.h"
#include "Core/Logging/LogMacros.h"
#include <algorithm> 

namespace
{
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

    // 2. 찾았다면 삭제를 진행합니다.
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

            if (!CompB /* || !CompB->bIsCollisionEnabled */)
                continue;

            // 겹침 판정
            if (CheckOverlap(CompA, CompB))
            {
                // 충돌 이벤트를 날림
                CompA->OnComponentOverlap(CompB);
                CompB->OnComponentOverlap(CompA);


                UE_LOG(Console, Info, "Collision!");
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


