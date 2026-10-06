#pragma once

#include "Core/Types/CoreTypes.h"
#include "Editor/UI/Panel/EditorPanelWidget.h"

// 활성 World 의 FWorldSettings 를 편집하는 ImGui 패널.
// 변경은 메모리상의 World 에 즉시 반영되고, scene save 시 .Scene 파일에 직렬화된다.
class EditorWorldSettingsWidget : public FEditorPanelWidget
{
public:
	void Render(const FEditorPanelContext& Context) override;

	bool bOpen = false;
};
