#pragma once

class FRmlUiRuntimeModule
{
public:
    bool Initialize();
    void Shutdown();

    bool IsInitialized() const { return bInitialized; }

private:
    bool bInitialized = false;
};
