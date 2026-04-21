#include "Render/Submission/Collect/DrawCollector.h"

// ==================== Public API ====================


void FDrawCollector::Reset()
{
    ResetCollectedPrimitives(CollectedSceneData.Primitives, true);
    ResetCollectedLights(CollectedSceneData.Lights);
    CollectedOverlayData.ClearTransientData();
}


// ==================== Reset Helpers ====================

void FDrawCollector::ResetCollectedPrimitives(FCollectedPrimitives& OutPrimitives, bool bClearOverlayTexts)
{
    OutPrimitives.VisibleProxies.clear();
    OutPrimitives.OpaqueProxies.clear();
    OutPrimitives.TransparentProxies.clear();

    if (bClearOverlayTexts)
    {
        OutPrimitives.OverlayTexts.clear();
    }
}

void FDrawCollector::ResetCollectedLights(FCollectedLights& OutLights)
{
    OutLights.GlobalLights = {};
    OutLights.LocalLights.clear();
}
