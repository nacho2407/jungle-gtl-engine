#include "ObjViewerStatWidget.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Engine/Core/CoreTypes.h"
#include "ImGui/imgui.h"

void FObjViewerStatWidget::Render(float DeltaTime)
{
	if (!Engine) return;

	// ControlWidget과 동일한 창 이름을 사용하여 내용 병합 (하나의 우측 패널로 렌더링)
	if (ImGui::Begin("ObjViewer Panel"))
	{
		if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto* MeshComp = Engine->GetPreviewMeshComponent();

			int32 VertexCount = 0;
			int32 TriangleCount = 0;

			if (MeshComp)
			{
				auto* Mesh = MeshComp->GetStaticMesh();
				VertexCount = static_cast<int32>(Mesh->GetVertices().size());
				// TriangleCount는 인덱스 수 / 3 (삼각형은 정점 3개로 구성)
				TriangleCount = static_cast<int32>(Mesh->GetIndices().size());
			}
			
			ImGui::Text("Vertices:  %d", VertexCount);
			ImGui::Text("Triangles: %d", TriangleCount);
		}
	}
	ImGui::End();
}