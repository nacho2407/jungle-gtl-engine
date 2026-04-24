// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Renderer.h"

#ifndef WITH_RENDER_MARKERS
#define WITH_RENDER_MARKERS 1
#endif

#if WITH_RENDER_MARKERS
#include <d3d11_1.h>

// FScopedGpuEvent는 렌더 영역의 핵심 동작을 담당합니다.
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
// FScopedGpuEvent는 렌더 영역의 핵심 동작을 담당합니다.
class FScopedGpuEvent
{
public:
    FScopedGpuEvent(FRenderer&, const wchar_t*) {}
};
#endif
