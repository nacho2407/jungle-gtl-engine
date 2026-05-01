#include "BoxProxy.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"

#include "Component/Shape/BoxComponent.h"

FBoxProxy::FBoxProxy(UBoxComponent* InComponent)
    : FShapeProxy(InComponent)
{
}

void FBoxProxy::VisualizeDebugLineInEditor(FScene& Scene, float DebugScale) const
{
    if (!Owner)
    {
        return;
    }

    UBoxComponent* Component = static_cast<UBoxComponent*>(Owner);
    FOBB           OBB       = Component->GetOBB();  
	FColor         Color     = Component->GetColor();

	FVector AxisX = OBB.Rotation.GetForwardVector(); // 앞(Forward) 방향
    FVector AxisY = OBB.Rotation.GetRightVector();   // 오른쪽(Right) 방향
    FVector AxisZ = OBB.Rotation.GetUpVector();      // 위(Up) 방향

    // 2. 각 방향 벡터에 박스의 크기(Extent)를 곱해 중심에서 뻗어나가는 실제 벡터를 만듭니다.
    FVector ExtX = AxisX * OBB.Extent.X;
    FVector ExtY = AxisY * OBB.Extent.Y;
    FVector ExtZ = AxisZ * OBB.Extent.Z;

	FVector P0 = OBB.Center + ExtX + ExtY + ExtZ; // Front-Right-Top
    FVector P1 = OBB.Center + ExtX - ExtY + ExtZ; // Front-Left-Top
    FVector P2 = OBB.Center - ExtX - ExtY + ExtZ; // Back-Left-Top
    FVector P3 = OBB.Center - ExtX + ExtY + ExtZ; // Back-Right-Top

    // 아랫면(Bottom Face) 4개 모서리 (-Z)
    FVector P4 = OBB.Center + ExtX + ExtY - ExtZ; // Front-Right-Bottom
    FVector P5 = OBB.Center + ExtX - ExtY - ExtZ; // Front-Left-Bottom
    FVector P6 = OBB.Center - ExtX - ExtY - ExtZ; // Back-Left-Bottom
    FVector P7 = OBB.Center - ExtX + ExtY - ExtZ; // Back-Right-Bottom

    // 4. 완성된 8개의 좌표를 렌더 함수에 던져줍니다.
    RenderDebugBox(Scene, P0, P1, P2, P3, P4, P5, P6, P7, Color);

}
