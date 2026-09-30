// Declares transient overlay line batches owned by the renderer for one frame.
#pragma once

#include "Render/Submission/Batching/LineBatch.h"

// FOverlayBatchSet holds the frame-local grid and debug line batches used by editor overlays.
struct FOverlayBatchSet
{
    FLineBatch GridLines;
    FLineBatch DebugLines;
};
