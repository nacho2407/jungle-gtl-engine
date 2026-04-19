#pragma once

#include "Render/Renderer/Renderer.h"

#ifndef WITH_RENDER_MARKERS
#define WITH_RENDER_MARKERS 1
#endif

#if WITH_RENDER_MARKERS
#include <d3d11_1.h>

class FScopedGpuEvent
{
public:
    FScopedGpuEvent(FRenderer& Renderer, const wchar_t* EventName)
    {
        Annotation = Renderer.GetFD3DDevice().GetUserDefinedAnnotation();
        if (Annotation && EventName)
        {
            Annotation->BeginEvent(EventName);
        }
    }

    ~FScopedGpuEvent()
    {
        if (Annotation)
        {
            Annotation->EndEvent();
        }
    }

private:
    ID3DUserDefinedAnnotation* Annotation = nullptr;
};
#else
class FScopedGpuEvent
{
public:
    FScopedGpuEvent(FRenderer&, const wchar_t*) {}
};
#endif
