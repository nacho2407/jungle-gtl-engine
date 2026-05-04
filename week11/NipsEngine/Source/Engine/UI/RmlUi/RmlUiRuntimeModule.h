#pragma once

class FRmlUiRuntimeModule
{
public:
    bool Initialize();
    void Shutdown();
    void ReleaseCachedFontRenderResources();

    bool IsInitialized() const { return bInitialized; }

private:
    bool bInitialized = false;
};
