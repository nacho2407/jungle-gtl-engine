#pragma once

#include "Core/CoreMinimal.h"

class ENGINE_API FViewport
{
  public:
    // 초기화 시 HWND와 크기를 받아 DX11 스왑체인과 연결
    void Initialize(HWND InWindowHandle, int32 InWidth, int32 InHeight);

    // 매 프레임 시작 시 호출 (ClearDepthStencil 등)
    void BeginRender();

    // 매 프레임 종료 시 호출 (Present)
    void EndRender();

    // 창 크기 변경 대응
    void OnResize(int32 NewWidth, int32 NewHeight);

    // 좌표 변환 함수 (Screen to World 등)
    Float2 ScreenToViewport(Float2 ScreenPos);

  private:

};
