#include "ObjViewerStatWidget.h"
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
			// TODO: MeshManager나 Renderer에서 실제 폴리곤 수를 가져오도록 연동
			int32 VertexCount = 0; 
			int32 TriangleCount = 0; 
			
			ImGui::Text("Vertices:  %d", VertexCount);
			ImGui::Text("Triangles: %d", TriangleCount);
		}
	}
	ImGui::End();
}