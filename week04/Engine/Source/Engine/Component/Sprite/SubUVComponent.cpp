#include "SubUVComponent.h"

namespace Engine::Component
{
    void USubUVComponent::SetFrameIndex(int32 InFrameIndex)
    {
        FrameIndex = (InFrameIndex >= 0) ? InFrameIndex : 0;

        const int32 FrameCount = GetFrameCount();
        if (FrameCount > 0 && FrameIndex >= FrameCount)
        {
            FrameIndex = FrameCount - 1;
        }
    }

    int32 USubUVComponent::GetFrameCount() const
    {
        return AtlasRows * AtlasColumns;
    }

    REGISTER_CLASS(Engine::Component, USubUVComponent)
} // namespace Engine::Component
