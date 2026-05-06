#include "Editor/UI/EditorMainPanel.h"

void FEditorMainPanel::OpenMaterialAsset(UMaterialInterface* Material)
{
    if (!Material)
    {
        return;
    }

    PanelVisibility.bShowMaterialEditor = true;
    Widgets.MaterialWidget.OpenMaterialAsset(Material);
}

void FEditorMainPanel::OpenMaterialSlot(UPrimitiveComponent* PrimitiveComp, int32 SlotIndex)
{
    if (!PrimitiveComp)
    {
        return;
    }

    PanelVisibility.bShowMaterialEditor = true;
    Widgets.MaterialWidget.OpenMaterialSlot(PrimitiveComp, SlotIndex);
}
